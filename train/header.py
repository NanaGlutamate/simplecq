import copy
import math

import torch

def abs(x):
    if isinstance(x, torch.Tensor):
        return torch.abs(x)
    return math.abs(x)

def acos(x):
    if isinstance(x, torch.Tensor):
        return torch.acos(x)
    return math.acos(x)

def asin(x):
    if isinstance(x, torch.Tensor):
        return torch.asin(x)
    return math.asin(x)

def atan(x):
    if isinstance(x, torch.Tensor):
        return torch.atan(x)
    return math.atan(x)

def atan2(y, x):
    if isinstance(x, torch.Tensor) or isinstance(y, torch.Tensor):
        return torch.atan2(y, x)
    return math.atan2(y, x)

def cos(x):
    if isinstance(x, torch.Tensor):
        return torch.cos(x)
    return math.cos(x)

def exp(x):
    if isinstance(x, torch.Tensor):
        return torch.exp(x)
    return math.exp(x)

def floor(x):
    if isinstance(x, torch.Tensor):
        return torch.floor(x)
    return math.floor(x)

def pow(x, n):
    if isinstance(x, torch.Tensor):
        return torch.pow(x, n)
    return math.pow(x, n)

def sin(x):
    if isinstance(x, torch.Tensor):
        return torch.sin(x)
    return math.sin(x)

def sqrt(x):
    if isinstance(x, torch.Tensor):
        return torch.sqrt(x)
    return math.sqrt(x)

def tan(x):
    if isinstance(x, torch.Tensor):
        return torch.tan(x)
    return math.tan(x)

F = torch.sigmoid
device = torch.device('cuda')

def length(x):
    return len(x)

def cot(x):
    return 1.0 / tan(x)

def fabs(x):
    return abs(x)

