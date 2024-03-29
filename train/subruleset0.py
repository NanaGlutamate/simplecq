from header import *


class SubRuleSet0:
    def __init__(self, input_vars, output_vars, cached_vars):
        self.input_vars = input_vars
        self.output_vars = output_vars
        self.cached_vars = cached_vars
        self.temp_cached_vars = copy.deepcopy(cached_vars)
        self.paras = []
        self.conditions = []
        self.actions = []
        self.state = None

        def action0(self: SubRuleSet0):
            i = 0
            head_idx = (- 1)
            enemy_idx = (- 1)
            side = self.input_vars["ForceSideID"]
            while (i < length(self.input_vars["carInfo"])):
                info = self.input_vars["carInfo"][i]["baseInfo"]
                if ((info["damageLevel"] == 0) or (info["damageLevel"] == 1)):
                    if ((side == info["side"]) and ((head_idx == (- 1)) or (info["id"] < self.input_vars["carInfo"][head_idx]["baseInfo"]["id"]))):
                        head_idx = i
                        pass
                    else:
                        if ((side != info["side"]) and ((enemy_idx == (- 1)) or (info["id"] < self.input_vars["carInfo"][enemy_idx]["baseInfo"]["id"]))):
                            enemy_idx = i
                            pass
                        else:
                            pass
                        pass
                    pass
                else:
                    pass
                i = (i + 1)
                
            i_0 = dict()
            i_0["a"] = head_idx
            i_0["b"] = enemy_idx
            self.temp_cached_vars["headIndex"] = i_0
            i_1 = None
            if ((self.temp_cached_vars["headIndex"]["a"] == (- 1)) or (self.temp_cached_vars["headIndex"]["b"] == (- 1))):
                i_1 = 1000000
                
            else:
                p_self = self.input_vars["carInfo"][self.temp_cached_vars["headIndex"]["a"]]["position"]
                p_enemy = self.input_vars["carInfo"][self.temp_cached_vars["headIndex"]["b"]]["position"]
                i_1 = sqrt((pow((p_self["y"] - p_enemy["y"]),2) + pow((p_self["x"] - p_enemy["x"]),2)))
            self.temp_cached_vars["distance"] = i_1
            i_2 = None
            if ((self.temp_cached_vars["headIndex"]["a"] == (- 1)) or (self.temp_cached_vars["headIndex"]["b"] == (- 1))):
                i_3 = dict()
                i_3["x"] = 1
                i_3["y"] = 0
                i_3["z"] = 0
                i_2 = i_3
                
            else:
                p_self = self.input_vars["carInfo"][self.temp_cached_vars["headIndex"]["a"]]["position"]
                p_enemy = self.input_vars["carInfo"][self.temp_cached_vars["headIndex"]["b"]]["position"]
                i_4 = dict()
                i_4["x"] = (p_enemy["x"] - p_self["x"])
                i_4["y"] = (p_enemy["y"] - p_self["y"])
                i_4["z"] = 0
                i_2 = i_4
            self.temp_cached_vars["enemy_direction"] = i_2
            0
            
        self.actions.append(action0)

        def action1(self):
            pass

        self.actions.append(action1)


        def condition0(self: SubRuleSet0):
            return self.state[self.index_0]
            
        self.conditions.append(condition0)


        self.index_0 = 0


    def cal_state(self):
        self.temp_cached_vars = copy.deepcopy(self.cached_vars)
        state = []

        state.append(1.0 if 1 else 0.0)

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
        table = [["distance", "enemy_direction", "headIndex"]]
        for vars in table[action]:
            self.cached_vars[vars] = self.temp_cached_vars[vars]
