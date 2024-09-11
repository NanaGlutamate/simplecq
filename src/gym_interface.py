import socket
from enum import Enum

def make_xml_data(data, type_def):
    if isinstance(type_def, str):
        atomic_type = type_def
        assert atomic_type in {
            "bool", "int8_t", "uint8_t", "int16_t", "uint16_t", 
            "int32_t", "uint32_t", "int64_t", "uint64_t", 
            "float", "double"
        }
        if atomic_type == 'bool': 
            return f'<{atomic_type}>{1 if data else 0}</{atomic_type}>'
        return f'<{atomic_type}>{data}</{atomic_type}>'

    elif isinstance(type_def, list):
        l = type_def
        return f'<li>{"".join(make_xml_data(value, l[0]) for value in data)}</li>'

    elif isinstance(type_def, dict):
        c = type_def
        return f'<c>{"".join(f"<{name}>{make_xml_data(value, c[name])}</{name}>" for name, value in data.items())}</c>'

    else:
        raise TypeError(f"Unsupported type definition: {type_def}")


class State(Enum):
    FIRST_INIT = 1
    RUNNING = 2
    END = 3

class Agent:
    def __init__(self, outputs_type, port, host='localhost', restart_key='restart', 
                 process_input=None, process_output=None, reward_func=None, end_func=None) -> None:
        '''
            outputs_type: 输出数据的类型标注信息.
                例子: {'location' : {'x' : 'double', 'y' : 'double', 'z' : 'double'}, 'enemiey_ids' : ['uint64_t']}
            
        '''
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((host, port))
        s.listen()
        self.link, _ = s.accept()
        self.outputs_type = outputs_type
        self.state = State.FIRST_INIT
        self.restart_key = restart_key

        self.process_input = (lambda x:x) if process_input is None else process_input
        self.process_output = (lambda x:x) if process_output is None else process_output
        self.cal_reward = (lambda x:0) if reward_func is None else reward_func
        self.cal_end = (lambda x:False) if end_func is None else end_func

        self.init_val = None

    def reset(self):
        if self.state == State.FIRST_INIT:
            self.state = State.RUNNING
            self.init_val = self._recv()
            return self.init_val
        elif self.state == State.END or self.state == State.RUNNING:
            self.state = State.RUNNING
            self._restart()
            self._recv()
            return self.init_val

    def step(self, action):
        '''
            注意: 由于CQSim执行模型的问题, step返回的state并不是当前传入的动作的结果, 而是上一个动作的结果
                即: 当前帧的action并不会影响step返回的state, 而只能对下一帧的step返回值产生影响
        '''
        assert self.state == State.RUNNING
        self._send(self.process_input(action))
        s = self.process_output(self._recv())
        end = self.cal_end(s)
        if end:
            self.state = State.END
        return s, self.cal_reward(s), end, ''

    def _restart(self):
        self.link.send(f'<c><{self.restart_key}><uint32_t>1</uint32_t></{self.restart_key}></c>\n'.encode())

    def _send(self, data):
        s = make_xml_data(data, self.outputs_type)
        self.link.send(s.encode() + b'\n')

    def _recv(self):
        s: bytes = b''
        while not s.endswith(b'\n'):
            s = s + self.link.recv(1024)
        return eval(s.decode())
