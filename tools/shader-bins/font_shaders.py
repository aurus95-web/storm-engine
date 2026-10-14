#!/usr/bin/env python3
"""Writes the precompiled pixel shaders of src/techniques/interfaces/font.fx.

On Windows the engine compiles font.fx with D3DX Effects. Other platforms have no HLSL
compiler at runtime and load "<fx name>_<function>.pso" files instead (see
CTechnique::GetShaderBinPath). These shaders are small enough to be encoded directly as
D3D9 ps_2_0 bytecode; the assembly is listed next to each instruction.

Register layout (matches the texture stages set by BmFont::Print):
    s0 - font bitmap, s1 - gradient texture, v0 - vertex color, t0 - texture coordinates

Usage: font_shaders.py <output directory>
"""

import struct
import sys
from pathlib import Path

# register types
TEMP, INPUT, CONST, TEXTURE, COLOROUT, SAMPLER = 0, 1, 2, 3, 8, 10

# opcodes
MOV, MUL, DCL, DEF, TEXLD = 0x01, 0x05, 0x1F, 0x51, 0x42

SWIZZLE_XYZW = 0xE4
SWIZZLE_XXXX = 0x00
SWIZZLE_ZZZZ = 0xAA


def reg_bits(reg_type, num):
    return 0x80000000 | ((reg_type & 7) << 28) | ((reg_type & 0x18) << 8) | num


def dst(reg_type, num, mask=0xF):
    return reg_bits(reg_type, num) | (mask << 16)


def src(reg_type, num, swizzle=SWIZZLE_XYZW):
    return reg_bits(reg_type, num) | (swizzle << 16)


def instr(opcode, *params):
    # ps_2_0 instruction tokens carry the number of parameter tokens in bits 24-27
    return [opcode | (len(params) << 24), *params]


def float_bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


DCL_TEXCOORD = instr(DCL, 0x80000000, dst(TEXTURE, 0, 0x7))  # dcl t0.xyz
DCL_COLOR = instr(DCL, 0x80000000, dst(INPUT, 0))  # dcl v0
DCL_SAMPLER0 = instr(DCL, 0x90000000, dst(SAMPLER, 0))  # dcl_2d s0
DCL_SAMPLER1 = instr(DCL, 0x90000000, dst(SAMPLER, 1))  # dcl_2d s1

PS_FONT = [
    0xFFFF0200,  # ps_2_0
    *DCL_TEXCOORD,
    *DCL_COLOR,
    *DCL_SAMPLER0,
    *instr(TEXLD, dst(TEMP, 0), src(TEXTURE, 0), src(SAMPLER, 0)),  # texld r0, t0, s0
    *instr(MUL, dst(TEMP, 0), src(TEMP, 0), src(INPUT, 0)),  # mul r0, r0, v0
    *instr(MOV, dst(COLOROUT, 0), src(TEMP, 0)),  # mov oC0, r0
    0x0000FFFF,  # end
]

PS_FONT_GRADIENT = [
    0xFFFF0200,  # ps_2_0
    *instr(DEF, dst(CONST, 0), *(float_bits(0.0),) * 4),  # def c0, 0, 0, 0, 0
    *DCL_TEXCOORD,
    *DCL_COLOR,
    *DCL_SAMPLER0,
    *DCL_SAMPLER1,
    *instr(TEXLD, dst(TEMP, 0), src(TEXTURE, 0), src(SAMPLER, 0)),  # texld r0, t0, s0
    *instr(MOV, dst(TEMP, 1), src(CONST, 0, SWIZZLE_XXXX)),  # mov r1, c0.x
    *instr(MOV, dst(TEMP, 1, 0x2), src(TEXTURE, 0, SWIZZLE_ZZZZ)),  # mov r1.y, t0.z
    *instr(TEXLD, dst(TEMP, 1), src(TEMP, 1), src(SAMPLER, 1)),  # texld r1, r1, s1
    *instr(MUL, dst(TEMP, 0), src(TEMP, 0), src(INPUT, 0)),  # mul r0, r0, v0
    *instr(MUL, dst(TEMP, 0), src(TEMP, 0), src(TEMP, 1)),  # mul r0, r0, r1
    *instr(MOV, dst(COLOROUT, 0), src(TEMP, 0)),  # mov oC0, r0
    0x0000FFFF,  # end
]


def main():
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    for name, tokens in (("font_ps_font.pso", PS_FONT), ("font_ps_font_gradient.pso", PS_FONT_GRADIENT)):
        (out_dir / name).write_bytes(struct.pack(f"<{len(tokens)}I", *tokens))
        print(f"{out_dir / name}: {len(tokens)} tokens")


if __name__ == "__main__":
    main()
