// Copyright 2024 RISC Zero, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

pub(crate) mod cpu;
#[cfg(feature = "cuda")]
pub(crate) mod cuda;

use std::rc::Rc;

use anyhow::Result;
use rand::thread_rng;
use risc0_core::scope;
use risc0_zkp::{
    adapter::{CircuitInfo as _, PROOF_SYSTEM_INFO},
    field::Elem as _,
    hal::{AccumPreflight, Buffer, CircuitHal, Hal},
    prove::Prover,
};

use super::{
    witgen::{preflight::PreflightTrace, WitnessGenerator},
    Seal, SegmentProver,
};
use crate::{
    execute::segment::Segment,
    zirgen::{
        circuit::{
            CircuitField, ExtVal, Val, REGCOUNT_ACCUM, REGCOUNT_MIX, REGISTER_GROUP_ACCUM,
            REGISTER_GROUP_CODE, REGISTER_GROUP_DATA,
        },
        taps::TAPSET,
        CircuitImpl,
    },
};

pub(crate) struct MetaBuffer<H: Hal> {
    pub buf: H::Buffer<H::Elem>,
    pub rows: usize,
    pub cols: usize,
    pub checked_reads: bool,
}

impl<H> MetaBuffer<H>
where
    H: Hal<Field = CircuitField, Elem = Val, ExtElem = ExtVal>,
{
    pub fn new(name: &'static str, hal: &H, rows: usize, cols: usize, checked_reads: bool) -> Self {
        let buf = hal.alloc_elem_init(name, rows * cols, Val::INVALID);
        Self {
            buf,
            rows,
            cols,
            checked_reads,
        }
    }

    #[cfg(test)]
    pub fn to_vec(&self) -> Vec<Val> {
        self.buf.to_vec()
    }
}

#[derive(Clone, Copy, PartialEq)]
pub(crate) enum StepMode {
    Parallel,
    #[cfg(test)]
    SeqForward,
    #[cfg(test)]
    SeqReverse,
}

pub(crate) trait CircuitWitnessGenerator<H: Hal> {
    fn generate_witness(
        &self,
        mode: StepMode,
        preflight: &PreflightTrace,
        global: &MetaBuffer<H>,
        data: &MetaBuffer<H>,
    ) -> Result<()>;
}

pub(crate) trait CircuitAccumulator<H: Hal> {
    fn step_accum(
        &self,
        preflight: &PreflightTrace,
        data: &MetaBuffer<H>,
        accum: &MetaBuffer<H>,
        mix: &MetaBuffer<H>,
    ) -> Result<()>;
}

pub(crate) struct SegmentProverImpl<H, C>
where
    H: Hal<Field = CircuitField, Elem = Val, ExtElem = ExtVal>,
    C: CircuitHal<H> + CircuitWitnessGenerator<H>,
{
    hal: Rc<H>,
    circuit_hal: Rc<C>,
}

impl<H, C> SegmentProverImpl<H, C>
where
    H: Hal<Field = CircuitField, Elem = Val, ExtElem = ExtVal>,
    C: CircuitHal<H> + CircuitWitnessGenerator<H>,
{
    pub fn new(hal: Rc<H>, circuit_hal: Rc<C>) -> Self {
        Self { hal, circuit_hal }
    }
}

impl<H, C> SegmentProver for SegmentProverImpl<H, C>
where
    H: Hal<Field = CircuitField, Elem = Val, ExtElem = ExtVal>,
    C: CircuitHal<H> + CircuitWitnessGenerator<H> + CircuitAccumulator<H>,
{
    fn prove(&self, segment: &Segment) -> Result<Seal> {
        scope!("prove");

        let mut rng = thread_rng();
        let rand_z = ExtVal::random(&mut rng);

        let witgen = WitnessGenerator::new(
            self.hal.as_ref(),
            self.circuit_hal.as_ref(),
            segment,
            StepMode::Parallel,
            rand_z,
        )?;

        let code = &witgen.code.buf;
        let data = &witgen.data.buf;
        let global = &witgen.global.buf;

        Ok(scope!("prove", {
            tracing::debug!("prove");

            let mut prover = Prover::new(self.hal.as_ref(), TAPSET);
            let hashfn = &self.hal.get_hash_suite().hashfn;

            let mix = scope!("main", {
                // At the start of the protocol, seed the Fiat-Shamir transcript with context information
                // about the proof system and circuit.
                prover
                    .iop()
                    .commit(&hashfn.hash_elem_slice(&PROOF_SYSTEM_INFO.encode()));
                prover
                    .iop()
                    .commit(&hashfn.hash_elem_slice(&CircuitImpl::CIRCUIT_INFO.encode()));

                // Concat globals and po2 into a vector.
                let global_len = global.size();
                let mut header = vec![Val::ZERO; global_len + 1];
                global.view_mut(|view| {
                    for (i, elem) in view.iter_mut().enumerate() {
                        *elem = elem.valid_or_zero();
                        header[i] = *elem;
                    }
                    header[global_len] = Val::new_raw(segment.po2);
                });

                let header_digest = hashfn.hash_elem_slice(&header);
                prover.iop().commit(&header_digest);
                prover.iop().write_field_elem_slice(header.as_slice());
                prover.set_po2(segment.po2 as usize);

                prover.commit_group(REGISTER_GROUP_CODE, code);
                prover.commit_group(REGISTER_GROUP_DATA, data);

                // Make the mixing values
                let mix: [Val; REGCOUNT_MIX] = std::array::from_fn(|_| prover.iop().random_elem());
                let mix = MetaBuffer {
                    buf: self.hal.copy_from_elem("mix", mix.as_slice()),
                    rows: 1,
                    cols: REGCOUNT_MIX,
                    checked_reads: true,
                };

                let accum = scope!(
                    "alloc(accum)",
                    MetaBuffer::new(
                        "accum",
                        self.hal.as_ref(),
                        witgen.cycles,
                        REGCOUNT_ACCUM,
                        true
                    )
                );

                self.circuit_hal
                    .step_accum(&witgen.trace, &witgen.data, &accum, &mix)?;

                scope!("zeroize(accum)", {
                    self.hal.eltwise_zeroize_elem(&accum.buf);
                });

                prover.commit_group(REGISTER_GROUP_ACCUM, &accum.buf);

                mix
            });

            prover.finalize(&[&mix.buf, global], self.circuit_hal.as_ref())
        }))
    }
}
