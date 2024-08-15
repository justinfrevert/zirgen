// Copyright (c) 2024 RISC Zero, Inc.
//
// All rights reserved.

#pragma once

#include "zirgen/Dialect/ZHLT/IR/ZHLT.h"
#include "zirgen/Dialect/ZStruct/IR/TypeUtils.h"

namespace zirgen::Zhlt {

using namespace mlir;

bool isLegalTypeArg(Attribute);
std::string mangledTypeName(StringRef componentName, llvm::ArrayRef<Attribute> typeArgs);

std::string mangledArrayTypeName(Type element, unsigned size);
std::string mangledTypeName(Type);

class StructBuilder {
public:
  StructBuilder(OpBuilder& builder, StringAttr name) : builder(builder), name(name) {}

  ZStruct::StructType getType() const {
    assert(members.size() == memberValues.size());
    for (auto& field : members) {
      assert(field.type);
    }
    return builder.getType<ZStruct::StructType>(name, members);
  }

  void addMember(StringRef memberName, Value value) {
    addMember(builder.getStringAttr(memberName), value);
  }

  void addMember(StringAttr memberName, Value value) {
    assert(value);
    assert(value.getType());

    if (memberName == "@super") {
      // TODO: Why do we require @super to be first?  Document this requirement.
      members.insert(members.begin(), {memberName, value.getType()});
      memberValues.insert(memberValues.begin(), value);
    } else {
      members.push_back({memberName, value.getType()});
      memberValues.push_back(value);
    }
  }

  bool empty() const { return members.empty(); }

  Value getValue(Location loc);

private:
  OpBuilder& builder;
  StringAttr name;
  SmallVector<ZStruct::FieldInfo> members;
  SmallVector<Value> memberValues;
};

/// Builds a layout structure.
class LayoutBuilder {
public:
  // Builds a layout structure with the given type name.  When the
  // layout is complete, "supplyLayout" must be called to supply a layout
  // value of the type returned by getType().
  LayoutBuilder(OpBuilder& builder, StringAttr typeName);

  /// Adds a new subcomponent construction of the given name.  Returns the value of the
  /// layout lookup to pass to the subcomponent.
  mlir::Value addMember(Location loc, StringRef memberName, mlir::Type type);

  /// Removes the given member name from this layout.  originalMemmber
  /// must match the value previously returned from addMember. Caller
  /// will need to remove any operations associated with it.
  void removeMember(Value originalMember, StringRef memberName);

  void setKind(ZStruct::LayoutKind kind) { this->kind = kind; }

  bool empty() { return members.empty(); }

  ZStruct::LayoutType getType();

  // Finalizes the layout that's been built.  If the layout is
  // non-empty, finalizeLayoutFunc is called to supply a layout value
  // that should be broken out for this layout's members.
  void supplyLayout(std::function<Value(/*layoutType=*/Type)> finalizeLayoutFunc);

private:
  OpBuilder& builder;
  StringAttr typeName;

  SmallVector<ZStruct::FieldInfo> members;

  // Placeholder for this layout until "supplyLayout" is called.
  Zhlt::MagicOp layoutPlaceholder;

  ZStruct::LayoutKind kind = ZStruct::LayoutKind::Normal;
};

/// Walks the super chains of the given components until it finds the most
/// "derived" common super type. A common super type must exist since all super
/// chains terminate in Component.
///
/// If "layout" is true, walk layout types instead of value types.
Type getLeastCommonSuper(TypeRange components, bool isLayout = false);

/// True iff the super chain of src contains dst.
bool isCoercibleTo(Type src, Type dst);

/// Coerces the given value to the given type. The type must be in the super
/// chain of value.
Value coerceTo(Value value, Type type, OpBuilder& builder);

/// If the given type is coercible to an array type, return that array type. If
/// it is not, return a null type.
ZStruct::ArrayType getCoercibleArrayType(Type type);

/// Coerces the given value to its closest super that is an array. This is
/// useful when lowering the source array of a MapOp, for example.
Value coerceToArray(Value value, OpBuilder& builder);

bool isGenericBuiltin(StringRef name);

using ZStruct::getComponentType;
using ZStruct::getEmptyLayoutType;
using ZStruct::getExtRefType;
using ZStruct::getNondetRegLayoutType;
using ZStruct::getNondetRegType;
using ZStruct::getRefType;
using ZStruct::getStringType;
using ZStruct::getTypeType;
using ZStruct::getValExtType;
using ZStruct::getValType;

Zll::ValType getFieldTypeOfValType(Type valType);

// Returns the type of this structure's "@super", if any.
Type getSuperType(Type subType, bool isLayout = false);

// Returns the component ID of this component.
std::string getTypeId(Type type);

/// A total order of ZHLT types so they can be used as keys in a std::map and
/// std::set
struct TypeCmp {
  bool operator()(const Type& lhs, const Type& rhs) const {
    // Assume all distinct types have distinct names. This should be a good
    // assumption thanks to our name mangling.
    return getTypeId(lhs) < getTypeId(rhs);
  }
};

// From a given layout type, extract the 'argument' types and their count
void extractArguments(llvm::MapVector<Type, size_t>& out, Type in);

// Extracts the maximum number of each 'argument' type used by any type in the range
llvm::MapVector<Type, size_t> muxArgumentCounts(TypeRange in);

// Extracts the maximum number of each 'argument' type used by any arm of the mux
llvm::MapVector<Type, size_t> muxArgumentCounts(ZStruct::LayoutType in);

// Return true if the given variable name is local
bool isLocalVariable(llvm::StringRef name);

} // namespace zirgen::Zhlt
