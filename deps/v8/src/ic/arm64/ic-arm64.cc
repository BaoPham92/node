// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/assembler-arm64-inl.h"
#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return al;
  }
}


bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address info_address = Assembler::return_address_from_call_start(address);

  InstructionSequence* patch_info = InstructionSequence::At(info_address);
  return patch_info->IsInlineData();
}


// Activate a SMI fast-path by patching the instructions generated by
// JumpPatchSite::EmitJumpIf(Not)Smi(), using the information encoded by
// JumpPatchSite::EmitPatchInfo().
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  // The patch information is encoded in the instruction stream using
  // instructions which have no side effects, so we can safely execute them.
  // The patch information is encoded directly after the call to the helper
  // function which is requesting this patch operation.
  Address info_address = Assembler::return_address_from_call_start(address);
  InlineSmiCheckInfo info(info_address);

  // Check and decode the patch information instruction.
  if (!info.HasSmiCheck()) {
    return;
  }

  if (FLAG_trace_ic) {
    LOG(isolate, PatchIC(address, info_address, info.SmiCheckDelta()));
  }

  // Patch and activate code generated by JumpPatchSite::EmitJumpIfNotSmi()
  // and JumpPatchSite::EmitJumpIfSmi().
  // Changing
  //   tb(n)z xzr, #0, <target>
  // to
  //   tb(!n)z test_reg, #0, <target>
  Instruction* to_patch = info.SmiCheck();
  PatchingAssembler patcher(isolate, reinterpret_cast<byte*>(to_patch), 1);
  DCHECK(to_patch->IsTestBranch());
  DCHECK(to_patch->ImmTestBranchBit5() == 0);
  DCHECK(to_patch->ImmTestBranchBit40() == 0);

  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  int branch_imm = to_patch->ImmTestBranch();
  Register smi_reg;
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(to_patch->Rt() == xzr.code());
    smi_reg = info.SmiRegister();
  } else {
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    DCHECK(to_patch->Rt() != xzr.code());
    smi_reg = xzr;
  }

  if (to_patch->Mask(TestBranchMask) == TBZ) {
    // This is JumpIfNotSmi(smi_reg, branch_imm).
    patcher.tbnz(smi_reg, 0, branch_imm);
  } else {
    DCHECK(to_patch->Mask(TestBranchMask) == TBNZ);
    // This is JumpIfSmi(smi_reg, branch_imm).
    patcher.tbz(smi_reg, 0, branch_imm);
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
