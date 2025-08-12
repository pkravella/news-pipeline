#!/usr/bin/env python3
import os, sys
import torch
from transformers import AutoTokenizer, AutoModelForSequenceClassification

def export(model_name: str, out_path: str, max_len: int = 256):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    tok = AutoTokenizer.from_pretrained(model_name)
    model = AutoModelForSequenceClassification.from_pretrained(model_name)
    model.eval()

    dummy = tok(["hello world"], return_tensors="pt", padding=True, truncation=True, max_length=max_len)
    with torch.no_grad():
        torch.onnx.export(
            model,
            (dummy["input_ids"], dummy["attention_mask"]),
            out_path,
            input_names=["input_ids","attention_mask"],
            output_names=["logits"],
            dynamic_axes={"input_ids":{0:"batch",1:"seq"}, "attention_mask":{0:"batch",1:"seq"}, "logits":{0:"batch"}},
            opset_version=17
        )
    print(f"Exported ONNX to {out_path}")

if __name__ == "__main__":
    model_name = sys.argv[1] if len(sys.argv)>1 else "yiyanghkust/finbert-tone"
    out_path   = sys.argv[2] if len(sys.argv)>2 else "models/sentiment.onnx"
    export(model_name, out_path)
