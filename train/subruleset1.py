from header import *


class SubRuleSet1:
    def __init__(self, input_vars, output_vars, cached_vars):
        self.input_vars = input_vars
        self.output_vars = output_vars
        self.cached_vars = cached_vars
        self.temp_cached_vars = copy.deepcopy(cached_vars)
        self.paras = []
        self.conditions = []
        self.actions = []
        self.state = None

        def action0(self: SubRuleSet1):
            self.temp_cached_vars["side_distance"] = (self.temp_cached_vars["side_distance"] + 10)
            0
            
        self.actions.append(action0)

        def action1(self: SubRuleSet1):
            self.temp_cached_vars["side_distance"] = (self.temp_cached_vars["side_distance"] - 10)
            1
            
        self.actions.append(action1)

        def action2(self: SubRuleSet1):
            self.temp_cached_vars["combatStage"] = 1
            self.output_vars["targetVel"] = 15
            self.output_vars["enableFire"] = 0.0
            self.output_vars["formationID"] = 5
            2
            
        self.actions.append(action2)

        def action3(self: SubRuleSet1):
            i_0 = dict()
            i_0["x"] = (self.temp_cached_vars["enemy_direction"]["x"] + self.temp_cached_vars["side_distance"])
            i_0["y"] = self.temp_cached_vars["enemy_direction"]["y"]
            i_0["z"] = 0
            self.output_vars["targetDir"] = i_0
            self.output_vars["targetVel"] = 15
            self.output_vars["enableFire"] = 0.0
            self.output_vars["formationID"] = 1
            3
            
        self.actions.append(action3)

        def action4(self: SubRuleSet1):
            self.temp_cached_vars["combatStage"] = 2
            self.output_vars["formationID"] = 2
            4
            
        self.actions.append(action4)

        def action5(self: SubRuleSet1):
            self.output_vars["targetDir"] = self.temp_cached_vars["enemy_direction"]
            5
            
        self.actions.append(action5)

        def action6(self: SubRuleSet1):
            self.temp_cached_vars["combatStage"] = 3
            self.output_vars["enableFire"] = 1.0
            6
            
        self.actions.append(action6)

        def action7(self: SubRuleSet1):
            self.output_vars["targetDir"] = self.temp_cached_vars["enemy_direction"]
            7
            
        self.actions.append(action7)

        def action8(self):
            pass

        self.actions.append(action8)


        def condition0(self: SubRuleSet1):
            return F(self.state[self.index_0] * self.p_0 + self.p_1)
            
        self.conditions.append(condition0)

        def condition1(self: SubRuleSet1):
            return F(self.state[self.index_1] * self.p_2 + self.p_3)
            
        self.conditions.append(condition1)

        def condition2(self: SubRuleSet1):
            return self.state[self.index_3] * (F(self.state[self.index_2] * self.p_4 + self.p_5))
            
        self.conditions.append(condition2)

        def condition3(self: SubRuleSet1):
            return self.state[self.index_4]
            
        self.conditions.append(condition3)

        def condition4(self: SubRuleSet1):
            return self.state[self.index_6] * (F(self.state[self.index_5] * self.p_6 + self.p_7))
            
        self.conditions.append(condition4)

        def condition5(self: SubRuleSet1):
            return self.state[self.index_7]
            
        self.conditions.append(condition5)

        def condition6(self: SubRuleSet1):
            return self.state[self.index_9] * (F(self.state[self.index_8] * self.p_8 + self.p_9))
            
        self.conditions.append(condition6)

        def condition7(self: SubRuleSet1):
            return self.state[self.index_10]
            
        self.conditions.append(condition7)

        self.p_0 = torch.tensor([1.00], device=device, requires_grad=True)
        self.paras.append(self.p_0)
        self.p_1 = torch.tensor([0.00], device=device, requires_grad=True)
        self.paras.append(self.p_1)
        self.p_2 = torch.tensor([1.00], device=device, requires_grad=True)
        self.paras.append(self.p_2)
        self.p_3 = torch.tensor([0.00], device=device, requires_grad=True)
        self.paras.append(self.p_3)
        self.p_4 = torch.tensor([1.00], device=device, requires_grad=True)
        self.paras.append(self.p_4)
        self.p_5 = torch.tensor([0.00], device=device, requires_grad=True)
        self.paras.append(self.p_5)
        self.p_6 = torch.tensor([1.00], device=device, requires_grad=True)
        self.paras.append(self.p_6)
        self.p_7 = torch.tensor([0.00], device=device, requires_grad=True)
        self.paras.append(self.p_7)
        self.p_8 = torch.tensor([1.00], device=device, requires_grad=True)
        self.paras.append(self.p_8)
        self.p_9 = torch.tensor([0.00], device=device, requires_grad=True)
        self.paras.append(self.p_9)

        self.index_0 = 0
        self.index_1 = 1
        self.index_2 = 2
        self.index_3 = 3
        self.index_4 = 4
        self.index_5 = 5
        self.index_6 = 6
        self.index_7 = 7
        self.index_8 = 8
        self.index_9 = 9
        self.index_10 = 10


    def cal_state(self):
        self.temp_cached_vars = copy.deepcopy(self.cached_vars)
        state = []

        state.append(((0.0 - 10) - self.temp_cached_vars["side_distance"]))
        state.append((self.temp_cached_vars["side_distance"] - 10))
        state.append((3000 - self.temp_cached_vars["distance"]))
        state.append(1.0 if (self.temp_cached_vars["combatStage"] == 0) else 0.0)
        state.append(1.0 if (self.temp_cached_vars["combatStage"] == 0) else 0.0)
        state.append((2000 - self.temp_cached_vars["distance"]))
        state.append(1.0 if (self.temp_cached_vars["combatStage"] == 1) else 0.0)
        state.append(1.0 if (self.temp_cached_vars["combatStage"] == 1) else 0.0)
        state.append((1500 - self.temp_cached_vars["distance"]))
        state.append(1.0 if (self.temp_cached_vars["combatStage"] == 2) else 0.0)
        state.append(1.0 if 1.0 else 0.0)

        self.state = torch.tensor(state, device=device, requires_grad=False)
        
    def forward(self, x=None):
        if x is not None:
            self.state = x
        prob = []
        p = torch.tensor([1.0], device=device, requires_grad=False)
        for con in self.conditions:
            p_this = p * con(self)
            prob.append(p_this)
            p = p - p_this
        prob.append(p)
        prob = torch.cat(prob)
        return prob
        

    def write_back(self, action):
        table = [["side_distance"], ["side_distance"], ["combatStage"], [], ["combatStage"], [], ["combatStage"], []]
        for vars in table[action]:
            self.cached_vars[vars] = self.temp_cached_vars[vars]
