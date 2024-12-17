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

use derive_more::Debug;
use risc0_binfmt::ExitCode;
use risc0_zkp::core::digest::Digest;
use serde::{Deserialize, Serialize};

use super::image::MemoryImage2;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Segment {
    /// Initial sparse memory state for the segment
    pub partial_image: MemoryImage2,

    pub pre_digest: Digest,

    pub post_digest: Digest,

    /// Recorded host->guest IO, one entry per read
    #[debug(skip)]
    pub read_record: Vec<Vec<u8>>,

    /// Recorded rlen of guest->host IO, one entry per write
    #[debug(skip)]
    pub write_record: Vec<u32>,

    pub user_cycles: u32,

    /// Cycle at which we suspend
    pub suspend_cycle: u32,

    /// Total paging cycles
    pub paging_cycles: u32,

    pub po2: u32,

    pub exit_code: ExitCode,

    pub index: u64,

    pub input_digest: Digest,

    pub output_digest: Option<Digest>,
}
