import socket
from enum import Enum

def make_xml_data(data, type_def) -> str:
    match type_def:
        case str(atomic_type):
            assert atomic_type in {"bool", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double"}
            return f'<{atomic_type}>{data}</{atomic_type}>'
        case list(l):
            return f'<li>{''.join(make_xml_data(value, l[0]) for value in data)}</li>'
        case dict(c):
            return f'<c>{''.join(f'<{name}>{make_xml_data(value, c[name])}</{name}>' for name, value in data)}</c>'

class State(Enum):
    FIRST_INIT = 1
    RUNNING = 2
    IS_END = 3

class Agent:
    def __init__(self, outputs_type, port, host='localhost', restart_key='restart', process_output=None, reward_func=None, end_func=None) -> None:
        '''
            outputs_type: type of output data, need for communicate with c++
                example for outputs_type: {'location' : {'x' : 'double', 'y' : 'double', 'z' : 'double'}, 'enemiey_ids' : ['uint64_t']}
            
        '''
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((host, port))
        s.listen()
        self.link, _ = s.accept()
        self.outputs_type = outputs_type
        self.state = State.FIRST_INIT
        self.restart_key = restart_key

        self.process_output = lambda x:x if process_output is None else process_output
        self.cal_reward = lambda x:0 if reward_func is None else reward_func
        self.cal_end = lambda x:False if end_func is None else end_func

    def reset(self):
        if self.state == State.FIRST_INIT:
            self.state = State.RUNNING
            return self._recv()
        elif self.state == State.IS_END or State.RUNNING:
            self.state = State.RUNNING
            self._restart()
            return self._recv()

    def step(self, action):
        assert self.state == State.RUNNING
        self._send(action)
        s = self.process_output(self._recv())
        end = self.cal_end(s)
        if end:
            self.state = State.IS_END
        return s, self.cal_reward(s), end, ''

    def _restart(self):
        self.link.send(f'<c><{self.restart_key}><uint32_t>1</uint32_t></{self.restart_key}></c>\n'.encode())

    def _send(self, data):
        s = make_xml_data(data, self.outputs_type)
        self.link.send(s.encode() + b'\n')

    def _recv(self)->str:
        s: bytes = b''
        while not s.endswith(b'\n'):
            s = s + self.link.recv(1024)
        return eval(s.decode())