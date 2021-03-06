// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CODE_STUBS_ARM64_H_
#define V8_ARM64_CODE_STUBS_ARM64_H_

namespace v8 {
namespace internal {


class RecordWriteStub: public PlatformCodeStub {
 public:
  // Stub to record the write of 'value' at 'address' in 'object'.
  // Typically 'address' = 'object' + <some offset>.
  // See MacroAssembler::RecordWriteField() for example.
  RecordWriteStub(Isolate* isolate,
                  Register object,
                  Register value,
                  Register address,
                  RememberedSetAction remembered_set_action,
                  SaveFPRegsMode fp_mode)
      : PlatformCodeStub(isolate),
        regs_(object,   // An input reg.
              address,  // An input reg.
              value) {  // One scratch reg.
    DCHECK(object.Is64Bits());
    DCHECK(value.Is64Bits());
    DCHECK(address.Is64Bits());
    minor_key_ = ObjectBits::encode(object.code()) |
                 ValueBits::encode(value.code()) |
                 AddressBits::encode(address.code()) |
                 RememberedSetActionBits::encode(remembered_set_action) |
                 SaveFPRegsModeBits::encode(fp_mode);
  }

  RecordWriteStub(uint32_t key, Isolate* isolate)
      : PlatformCodeStub(key, isolate), regs_(object(), address(), value()) {}

  enum Mode {
    STORE_BUFFER_ONLY,
    INCREMENTAL,
    INCREMENTAL_COMPACTION
  };

  bool SometimesSetsUpAFrame() override { return false; }

  static Mode GetMode(Code* stub);

  static void Patch(Code* stub, Mode mode);

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();

 private:
  // This is a helper class to manage the registers associated with the stub.
  // The 'object' and 'address' registers must be preserved.
  class RegisterAllocation {
   public:
    RegisterAllocation(Register object, Register address, Register scratch);

    void Save(MacroAssembler* masm) {
      // We don't have to save scratch0_ because it was given to us as
      // a scratch register.
      masm->Push(scratch1_, scratch2_);
    }

    void Restore(MacroAssembler* masm) {
      masm->Pop(scratch2_, scratch1_);
    }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      // TODO(all): This can be very expensive, and it is likely that not every
      // register will need to be preserved. Can we improve this?
      masm->PushCPURegList(saved_regs_);
      if (mode == kSaveFPRegs) {
        masm->PushCPURegList(saved_fp_regs_);
      }
    }

    void RestoreCallerSaveRegisters(MacroAssembler*masm, SaveFPRegsMode mode) {
      // TODO(all): This can be very expensive, and it is likely that not every
      // register will need to be preserved. Can we improve this?
      if (mode == kSaveFPRegs) {
        masm->PopCPURegList(saved_fp_regs_);
      }
      masm->PopCPURegList(saved_regs_);
    }

    Register object() { return object_; }
    Register address() { return address_; }
    Register scratch0() { return scratch0_; }
    Register scratch1() { return scratch1_; }
    Register scratch2() { return scratch2_; }

   private:
    Register object_;
    Register address_;
    Register scratch0_;
    Register scratch1_ = NoReg;
    Register scratch2_ = NoReg;
    CPURegList saved_regs_;
    CPURegList saved_fp_regs_;

    // TODO(all): We should consider moving this somewhere else.
    static CPURegList GetValidRegistersForAllocation() {
      // The list of valid registers for allocation is defined as all the
      // registers without those with a special meaning.
      //
      // The default list excludes registers x26 to x31 because they are
      // reserved for the following purpose:
      //  - x26 root register
      //  - x27 context pointer register
      //  - x28 jssp
      //  - x29 frame pointer
      //  - x30 link register(lr)
      //  - x31 xzr/stack pointer
      CPURegList list(CPURegister::kRegister, kXRegSizeInBits, 0, 25);

      // We also remove MacroAssembler's scratch registers.
      list.Remove(MacroAssembler::DefaultTmpList());

      return list;
    }

    friend class RecordWriteStub;
  };

  enum OnNoNeedToInformIncrementalMarker {
    kReturnOnNoNeedToInformIncrementalMarker,
    kUpdateRememberedSetOnNoNeedToInformIncrementalMarker
  };

  inline Major MajorKey() const final { return RecordWrite; }

  void Generate(MacroAssembler* masm) override;
  void GenerateIncremental(MacroAssembler* masm, Mode mode);
  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm);

  void Activate(Code* code) override;

  Register object() const {
    return Register::from_code(ObjectBits::decode(minor_key_));
  }

  Register value() const {
    return Register::from_code(ValueBits::decode(minor_key_));
  }

  Register address() const {
    return Register::from_code(AddressBits::decode(minor_key_));
  }

  RememberedSetAction remembered_set_action() const {
    return RememberedSetActionBits::decode(minor_key_);
  }

  SaveFPRegsMode save_fp_regs_mode() const {
    return SaveFPRegsModeBits::decode(minor_key_);
  }

  class ObjectBits: public BitField<int, 0, 5> {};
  class ValueBits: public BitField<int, 5, 5> {};
  class AddressBits: public BitField<int, 10, 5> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 15, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 16, 1> {};

  Label slow_;
  RegisterAllocation regs_;
};


// Helper to call C++ functions from generated code. The caller must prepare
// the exit frame before doing the call with GenerateCall.
class DirectCEntryStub: public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  bool NeedsImmovableCode() override { return true; }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DirectCEntry, PlatformCodeStub);
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  explicit NameDictionaryLookupStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateNegativeLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register receiver,
                                     Register properties,
                                     Handle<Name> name,
                                     Register scratch0);

  bool SometimesSetsUpAFrame() override { return false; }

 private:
  static const int kInlinedProbes = 4;
  static const int kTotalProbes = 20;

  static const int kCapacityOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(NameDictionaryLookup, PlatformCodeStub);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_CODE_STUBS_ARM64_H_
