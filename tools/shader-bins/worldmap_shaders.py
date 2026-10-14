#!/usr/bin/env python3
"""Writes the precompiled vertex shader of src/techniques/worldmap/worldmap.fx.

On Windows the engine compiles worldmap.fx with D3DX Effects. Other platforms have no HLSL
compiler at runtime and load "<fx name>_<function>.vso" files instead (see
CTechnique::GetShaderBinPath). The shader is small enough to be encoded directly as D3D9
vs_1_1 bytecode; the assembly is listed next to each instruction.

HLSL (worldmap.fx, function main, used by the WdmClouds technique):
    vOut.position = mul(vPosition, projectionMatrix);
    vOut.diffuse = vColor.xxxw;
    vOut.specular = vColor.yyyw * cloudMatrix[2];
    vOut.uv0 = vTexCoord;
    float cloud_size = vColor.z * cloudMatrix[1].z + cloudMatrix[1].x;
    float view_angle = dot(normalize(vPosition), normalize(cloudMatrix[0]));
    vOut.uv1 = view_angle * rcp(cloud_size) + cloudMatrix[1].w;

Register layout (matches WdmClouds::Draw):
    c0-c3 - projectionMatrix rows, c4-c6 - cloudMatrix rows
    v0 - position (w = 1), v1 - color, v2 - texture coordinates

Usage: worldmap_shaders.py <output directory>
"""

import sys
from pathlib import Path

# register types
TEMP, INPUT, CONST, RASTOUT, ATTROUT, TEXCRDOUT = 0, 1, 2, 4, 5, 6

# opcodes
MOV, ADD, MAD, MUL, RCP, RSQ, DP4, DCL = 0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x09, 0x1F

# declaration usages
USAGE_POSITION, USAGE_TEXCOORD, USAGE_COLOR = 0, 5, 10

XYZW, XXXX, YYYY, ZZZZ, WWWW = 0xE4, 0x00, 0x55, 0xAA, 0xFF
XXXW, YYYW = 0xC0, 0xD5
MASK_W = 0x8


def reg_bits(reg_type, num):
    return 0x80000000 | ((reg_type & 7) << 28) | ((reg_type & 0x18) << 8) | num


def dst(reg_type, num, mask=0xF):
    return reg_bits(reg_type, num) | (mask << 16)


def src(reg_type, num, swizzle=XYZW):
    return reg_bits(reg_type, num) | (swizzle << 16)


def instr(opcode, *params):
    # vs_1_1 instruction tokens carry no length
    return [opcode, *params]


def dcl(usage, num):
    return instr(DCL, 0x80000000 | usage, dst(INPUT, num))


VS_MAIN = [
    0xFFFE0101,  # vs_1_1
    *dcl(USAGE_POSITION, 0),  # dcl_position v0
    *dcl(USAGE_COLOR, 1),  # dcl_color v1
    *dcl(USAGE_TEXCOORD, 2),  # dcl_texcoord v2
    *instr(MOV, dst(TEMP, 0), src(INPUT, 0)),  # mov r0, v0
    *instr(MUL, dst(TEMP, 1), src(TEMP, 0, XXXX), src(CONST, 0)),  # mul r1, r0.x, c0
    *instr(MAD, dst(TEMP, 1), src(TEMP, 0, YYYY), src(CONST, 1), src(TEMP, 1)),  # mad r1, r0.y, c1, r1
    *instr(MAD, dst(TEMP, 1), src(TEMP, 0, ZZZZ), src(CONST, 2), src(TEMP, 1)),  # mad r1, r0.z, c2, r1
    *instr(ADD, dst(RASTOUT, 0), src(TEMP, 1), src(CONST, 3)),  # add oPos, r1, c3
    *instr(MOV, dst(ATTROUT, 0), src(INPUT, 1, XXXW)),  # mov oD0, v1.xxxw
    *instr(MUL, dst(ATTROUT, 1), src(INPUT, 1, YYYW), src(CONST, 6)),  # mul oD1, v1.yyyw, c6
    *instr(MOV, dst(TEXCRDOUT, 0), src(INPUT, 2)),  # mov oT0, v2
    *instr(MOV, dst(TEMP, 2), src(CONST, 5)),  # mov r2, c5
    *instr(MAD, dst(TEMP, 2, MASK_W), src(INPUT, 1, ZZZZ), src(TEMP, 2, ZZZZ), src(TEMP, 2, XXXX)),  # mad r2.w, v1.z, r2.z, r2.x
    *instr(RCP, dst(TEMP, 2, MASK_W), src(TEMP, 2, WWWW)),  # rcp r2.w, r2.w
    *instr(DP4, dst(TEMP, 3, MASK_W), src(TEMP, 0), src(TEMP, 0)),  # dp4 r3.w, r0, r0
    *instr(RSQ, dst(TEMP, 3, MASK_W), src(TEMP, 3, WWWW)),  # rsq r3.w, r3.w
    *instr(MUL, dst(TEMP, 3), src(TEMP, 0), src(TEMP, 3, WWWW)),  # mul r3, r0, r3.w
    *instr(DP4, dst(TEMP, 4, MASK_W), src(CONST, 4), src(CONST, 4)),  # dp4 r4.w, c4, c4
    *instr(RSQ, dst(TEMP, 4, MASK_W), src(TEMP, 4, WWWW)),  # rsq r4.w, r4.w
    *instr(MUL, dst(TEMP, 4), src(CONST, 4), src(TEMP, 4, WWWW)),  # mul r4, c4, r4.w
    *instr(DP4, dst(TEMP, 1, MASK_W), src(TEMP, 3), src(TEMP, 4)),  # dp4 r1.w, r3, r4
    *instr(MAD, dst(TEMP, 1, MASK_W), src(TEMP, 1, WWWW), src(TEMP, 2, WWWW), src(CONST, 5, WWWW)),  # mad r1.w, r1.w, r2.w, c5.w
    *instr(MOV, dst(TEXCRDOUT, 1), src(TEMP, 1, WWWW)),  # mov oT1, r1.w
    0x0000FFFF,  # end
]


def write(path, tokens):
    path.write_bytes(b"".join(token.to_bytes(4, "little") for token in tokens))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    write(out / "worldmap_main.vso", VS_MAIN)
