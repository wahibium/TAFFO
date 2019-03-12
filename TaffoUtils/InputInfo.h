//===-- InputInfo.h - Data Structures for Input Info Metadata ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Various data structures that support in-memory representation of
/// input info metadata.
///
//===----------------------------------------------------------------------===//

#ifndef TAFFO_INPUT_INFO_H
#define TAFFO_INPUT_INFO_H

#include <memory>
#include <sstream>
#include "llvm/Support/Casting.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"

namespace mdutils {

#define FIXP_TYPE_FLAG "fixp"

/// Info about a data type for numerical computations.
class TType {
public:
  enum TTypeKind { K_FPType };

  TType(TTypeKind K) : Kind(K) {}

  virtual double getRoundingError() const = 0;

  /// Safe approximation of the minimum value representable with this Type.
  virtual double getMinValueBound() const = 0;
  /// Safe approximation of the maximum value representable with this Type.
  virtual double getMaxValueBound() const = 0;

  virtual llvm::MDNode *toMetadata(llvm::LLVMContext &C) const = 0;
  
  virtual TType *clone() const = 0;

  virtual ~TType() = default;

  static std::unique_ptr<TType> createFromMetadata(llvm::MDNode *MDN);
  static bool isTTypeMetadata(llvm::Metadata *MD);
  
  virtual std::string toString() const {
    return "TType";
  };
  
  virtual bool operator ==(const TType &b) const {
    return Kind == b.Kind;
  }

  TTypeKind getKind() const { return Kind; }
private:
  const TTypeKind Kind;
};

/// A Fixed Point Type.
/// Contains bit width, number of fractional bits of the format
/// and whether it is signed or not.
class FPType : public TType {
public:
  FPType(unsigned Width, unsigned PointPos, bool Signed = true)
    : TType(K_FPType), Width((Signed) ? -Width : Width), PointPos(PointPos) {}

  FPType(int Width, unsigned PointPos)
    : TType(K_FPType), Width(Width), PointPos(PointPos) {}

  double getRoundingError() const override;
  double getMinValueBound() const override;
  double getMaxValueBound() const override;
  llvm::MDNode *toMetadata(llvm::LLVMContext &C) const override;
  unsigned getWidth() const { return std::abs(Width); }
  int getSWidth() const { return Width; }
  unsigned getPointPos() const { return PointPos; }
  bool isSigned() const { return Width < 0; }
  
  virtual TType *clone() const override {
    return new FPType(Width, PointPos);
  };

  static bool isFPTypeMetadata(llvm::MDNode *MDN);
  static std::unique_ptr<FPType> createFromMetadata(llvm::MDNode *MDN);
  
  virtual std::string toString() const override {
    std::stringstream stm;
    if (Width < 0)
      stm << "s";
    else
      stm << "u";
    stm << std::abs(Width)-PointPos << "_" << PointPos << "fixp";
    return stm.str();
  };
  
  virtual bool operator ==(const TType &b) const override {
    if (!TType::operator==(b))
      return false;
    const FPType *b2 = llvm::cast<FPType>(&b);
    return Width == b2->Width && PointPos == b2->PointPos;
  }

  static bool classof(const TType *T) { return T->getKind() == K_FPType; }
protected:
  int Width; ///< Width of the format (in bits), negative if signed.
  unsigned PointPos; ///< Number of fractional bits.
};

struct Range {
public:
  double Min;
  double Max;

  Range() : Min(0.0), Max(0.0) {}
  Range(double Min, double Max) : Min(Min), Max(Max) {}
  Range(Range& r) : Min(r.Min), Max(r.Max) {}

  llvm::MDNode *toMetadata(llvm::LLVMContext &C) const;
  static std::unique_ptr<Range> createFromMetadata(llvm::MDNode *MDN);
  static bool isRangeMetadata(llvm::Metadata *MD);
};

std::unique_ptr<double> CreateInitialErrorFromMetadata(llvm::MDNode *MDN);
llvm::MDNode *InitialErrorToMetadata(double Error);
bool IsInitialErrorMetadata(llvm::Metadata *MD);

class MDInfo {
public:
  enum MDInfoKind { K_Struct, K_Field };

  MDInfo(MDInfoKind K) : Kind(K) {}

  virtual llvm::MDNode *toMetadata(llvm::LLVMContext &C) const = 0;
  
  virtual MDInfo *clone() const = 0;

  virtual ~MDInfo() = default;
  MDInfoKind getKind() const { return Kind; }
  
  virtual std::string toString() const {
    return "MDInfo";
  };

private:
  const MDInfoKind Kind;
};

/// Structure containing pointers to Type, Range, and initial Error
/// of an LLVM Value.
struct InputInfo : public MDInfo {
  std::shared_ptr<TType> IType;
  std::shared_ptr<Range> IRange;
  std::shared_ptr<double> IError;

  InputInfo()
    : MDInfo(K_Field), IType(nullptr), IRange(nullptr), IError(nullptr) {}

  InputInfo(std::shared_ptr<TType> T, std::shared_ptr<Range> R, std::shared_ptr<double> Error)
    : MDInfo(K_Field), IType(T), IRange(R), IError(Error) {}
  
  virtual MDInfo *clone() const override {
    std::shared_ptr<TType> NewIType(IType.get() ? IType->clone() : nullptr);
    std::shared_ptr<Range> NewIRange(IRange.get() ? new Range(*IRange) : nullptr);
    std::shared_ptr<double> NewIError(IError.get() ? new double(*IError) : nullptr);
    return new InputInfo(NewIType, NewIRange, NewIError);
  }

  llvm::MDNode *toMetadata(llvm::LLVMContext &C) const override;
  static bool isInputInfoMetadata(llvm::Metadata *MD);

  InputInfo &operator=(const InputInfo &O) {
    assert(this->getKind() == O.getKind());
    this->IType = O.IType;
    this->IRange = O.IRange;
    this->IError = O.IError;
    return *this;
  };
  
  virtual std::string toString() const override {
    std::stringstream sstm;
    sstm << "scalar(";
    bool first = true;
    if (IType.get()) {
      first = false;
      sstm << "type(" << IType->toString() << ")";
    }
    if (IRange.get()) {
      if (!first) sstm << " "; else first = false;
      sstm << "range(" << IRange->Min << ", " << IRange->Max << ")";
    }
    if (IError.get()) {
      if (!first) sstm << " ";
      sstm << "error(" << *IError << ")";
    }
    sstm << ")";
    return sstm.str();
  };

  static bool classof(const MDInfo *M) { return M->getKind() == K_Field; }
};

class StructInfo : public MDInfo {
private:
  typedef llvm::SmallVector<std::shared_ptr<MDInfo>, 4U> FieldsType;
  FieldsType Fields;

public:
  typedef FieldsType::iterator iterator;
  typedef FieldsType::const_iterator const_iterator;
  typedef FieldsType::size_type size_type;

  StructInfo(int size)
    : MDInfo(K_Struct), Fields(size, nullptr) {}
  
  StructInfo(const llvm::ArrayRef<std::shared_ptr<MDInfo>> SInfos)
    : MDInfo(K_Struct), Fields(SInfos.begin(), SInfos.end()) {}

  iterator begin() { return Fields.begin(); }
  iterator end() { return Fields.end(); }
  const_iterator begin() const { return Fields.begin(); }
  const_iterator end() const { return Fields.end(); }
  size_type size() const { return Fields.size(); }
  MDInfo *getField(size_type I) const { return Fields[I].get(); }
  std::shared_ptr<MDInfo> getField(size_type I) { return Fields[I]; }
  
  /** Builds a StructInfo with the recursive structure of the specified
   *  LLVM Type. All non-struct struct members are set to nullptr.
   *  @returns Either a StructInfo, or nullptr if the type does not
   *    contain any structure. */
  static std::shared_ptr<StructInfo> constructFromLLVMType(llvm::Type *t) {
    int c = t->getNumContainedTypes();
    if (c == 0)
      return nullptr;
    if (t->isStructTy()) {
      FieldsType fields;
      for (int i=0; i<c; i++)
        fields.push_back(StructInfo::constructFromLLVMType(t->getContainedType(i)));
      return std::make_shared<StructInfo>(StructInfo(fields));
    }
    return StructInfo::constructFromLLVMType(t->getContainedType(0));
  }
  
  std::shared_ptr<MDInfo> resolveFromIndexList(llvm::Type *type, llvm::ArrayRef<unsigned> indices) {
    llvm::Type *resolvedType = type;
    std::shared_ptr<MDInfo> resolvedInfo(this);
    for (unsigned idx: indices) {
      if (resolvedInfo.get() == nullptr)
        break;
      if (resolvedType->isStructTy()) {
        resolvedType = resolvedType->getContainedType(idx);
        resolvedInfo = llvm::cast<StructInfo>(resolvedInfo.get())->getField(idx);
      } else {
        resolvedType = resolvedType->getContainedType(idx);
      }
    }
    return resolvedInfo;
  }
  
  virtual MDInfo *clone() const override {
    FieldsType newFields;
    for (std::shared_ptr<MDInfo> oldF: Fields) {
      if (oldF.get())
        newFields.push_back(std::shared_ptr<MDInfo>(oldF->clone()));
      else
        newFields.push_back(std::shared_ptr<MDInfo>(nullptr));
    }
    return new StructInfo(newFields);
  }
  
  virtual std::string toString() const override {
    std::stringstream sstm;
    sstm << "struct(";
    bool first = true;
    for (std::shared_ptr<MDInfo> i: Fields) {
      if (!first)
        sstm << ", ";
      if (i.get()) {
        sstm << i->toString();
      } else {
        sstm << "void()";
      }
      first = false;
    }
    sstm << ")";
    return sstm.str();
  };

  llvm::MDNode *toMetadata(llvm::LLVMContext &C) const override;

  static bool classof(const MDInfo *M) { return M->getKind() == K_Struct; }
};


/// Struct containing info about a possible comparison error.
struct CmpErrorInfo {
public:
  double MaxTolerance; ///< Maximum error tolerance for this comparison.
  bool MayBeWrong; ///< True if this comparison may be wrong due to propagated errors.

  CmpErrorInfo(double MaxTolerance, bool MayBeWrong = true)
    : MaxTolerance(MaxTolerance), MayBeWrong(MayBeWrong) {}

  llvm::MDNode *toMetadata(llvm::LLVMContext &C) const;

  static std::unique_ptr<CmpErrorInfo> createFromMetadata(llvm::MDNode *MDN);
};


bool IsNullInputInfoField(llvm::Metadata *MD);

llvm::MDNode *createDoubleMDNode(llvm::LLVMContext &C, double Value);
double retrieveDoubleMDNode(llvm::MDNode *MDN);

} // end namespace mdutils

#endif