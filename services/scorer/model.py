import os, time
from typing import List, Dict, Tuple
import numpy as np

# ONNX
import onnxruntime as ort

# Transformers fallback
from transformers import AutoTokenizer, AutoModelForSequenceClassification
import torch

LABELS_FINBERT = ["negative", "neutral", "positive"]

def softmax(x: np.ndarray) -> np.ndarray:
    x = x - np.max(x, axis=-1, keepdims=True)
    e = np.exp(x)
    return e / np.sum(e, axis=-1, keepdims=True)

class SentimentModel:
    """
    If onnx_path exists -> ONNX Runtime.
    Else -> Transformers PyTorch model.
    Outputs: sentiment in [-1,1], confidence in [0,1].
    """
    def __init__(self, model_name: str, onnx_path: str, use_onnx_if_available: bool = True):
        self.model_name = model_name
        self.onnx_path = onnx_path
        self.use_onnx = use_onnx_if_available and os.path.exists(onnx_path)
        self.tokenizer = AutoTokenizer.from_pretrained(model_name)

        if self.use_onnx:
            self.sess = ort.InferenceSession(onnx_path, providers=["CPUExecutionProvider"])
            self.input_names = [i.name for i in self.sess.get_inputs()]
        else:
            self.model = AutoModelForSequenceClassification.from_pretrained(model_name)
            self.model.eval()
            if torch.cuda.is_available():
                self.model.to("cuda")

    def encode_batch(self, texts: List[str], max_len: int = 256) -> Dict[str, np.ndarray]:
        toks = self.tokenizer(texts, return_tensors="np", padding=True, truncation=True, max_length=max_len)
        # Convert int64 -> int32 for ONNX Runtime compatibility in some envs
        for k in toks:
            if toks[k].dtype == np.int64:
                toks[k] = toks[k].astype(np.int32)
        return toks

    def sentiment_batch(self, texts: List[str]) -> Tuple[np.ndarray, np.ndarray]:
        if len(texts) == 0:
            return np.zeros((0,), dtype=np.float32), np.zeros((0,), dtype=np.float32)

        toks = self.encode_batch(texts)
        if self.use_onnx:
            logits = self.sess.run(None, {self.input_names[0]: toks["input_ids"],
                                          self.input_names[1]: toks["attention_mask"]})[0]
        else:
            with torch.no_grad():
                ids = torch.tensor(toks["input_ids"])
                mask = torch.tensor(toks["attention_mask"])
                if torch.cuda.is_available():
                    ids = ids.to("cuda"); mask = mask.to("cuda")
                logits = self.model(input_ids=ids, attention_mask=mask).logits.cpu().numpy()

        probs = softmax(logits)
        # Map FinBERT labels to score in [-1,1]
        # score = P(positive) - P(negative); confidence = 1 - entropy approx using max prob
        p_neg = probs[:, 0]; p_neu = probs[:, 1]; p_pos = probs[:, 2]
        score = (p_pos - p_neg).astype(np.float32)
        conf = np.max(probs, axis=-1).astype(np.float32)
        return score, conf

def rule_topics(texts: List[str]) -> List[List[str]]:
    out = []
    for t in texts:
        s = (t or "").lower()
        cur = set()
        def anyof(words): return any(w in s for w in words)
        if anyof(["guidance","outlook","forecast"]): cur.add("guidance")
        if anyof(["earnings","q1","q2","q3","q4","fiscal","full-year","eps","revenue","beat","miss"]): cur.add("earnings")
        if anyof(["acquire","acquisition","merger","m&a","takeover"]): cur.add("m&a")
        if anyof(["lawsuit","litigation","settlement","sues","sued","investigation"]): cur.add("legal")
        if anyof(["downgrade","upgrade","initiates coverage","price target"]): cur.add("rating/analyst")
        if anyof(["macro","inflation","cpi","jobs report","fed","interest rates","treasury"]): cur.add("macro")
        if anyof(["product","launches","introduces","unveils","chip","ai model","device","phone","gpu"]): cur.add("product")
        out.append(sorted(cur))
    return out
