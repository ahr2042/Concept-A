#!/usr/bin/env python3
"""Rewrite an FP16 ONNX graph as FP32.

The published yolov5n.onnx is an FP16 graph. Feeding that straight through
onnx2ncnn produces a model whose Convolution weight_data comes out null, which
then SIGSEGVs in ncnn's Vulkan conv-pipeline creation (it loads fine on ncnn's
CPU path, which is what made the bug so confusing). Converting to FP32 first —
then letting pnnx emit the ncnn model — yields weights that ncnn's Vulkan
backend can actually use. See scripts/onnx-to-ncnn.sh.

Usage: fp16to32.py <in.onnx> <out.onnx>
"""
import sys
import numpy as np
import onnx
from onnx import numpy_helper, TensorProto

src, dst = sys.argv[1], sys.argv[2]
m = onnx.load(src)
g = m.graph
F16, F32 = TensorProto.FLOAT16, TensorProto.FLOAT

# 1) initializers (the weights) fp16 -> fp32
for init in g.initializer:
    if init.data_type == F16:
        arr = numpy_helper.to_array(init).astype(np.float32)
        init.CopyFrom(numpy_helper.from_array(arr, init.name))

# 2) graph input/output/value_info tensor element types
def fix(vis):
    for vi in vis:
        tt = vi.type.tensor_type
        if tt.elem_type == F16:
            tt.elem_type = F32
fix(g.input); fix(g.output); fix(g.value_info)

# 3) Cast-to-fp16 nodes -> cast to fp32, and any fp16 attribute tensors
for node in g.node:
    if node.op_type == "Cast":
        for a in node.attribute:
            if a.name == "to" and a.i == F16:
                a.i = F32
    for a in node.attribute:
        if a.type == onnx.AttributeProto.TENSOR and a.t.data_type == F16:
            arr = numpy_helper.to_array(a.t).astype(np.float32)
            a.t.CopyFrom(numpy_helper.from_array(arr))

onnx.save(m, dst)
print("wrote", dst)
