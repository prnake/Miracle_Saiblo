EVENT_ID = 0

class Event:
    def __init__(self,name,parameter_dict = {},priority = 0):
        global EVENT_ID
        self.id = EVENT_ID
        EVENT_ID += 1
        self.name = name
        self.priority = priority
        self.parameter_dict = parameter_dict

    def __lt__(self,other):
        return (self.priority < other.priority) or \
            (self.priority == other.priority and self.id < other.id)

    def __str__(self):
        return '''Event: {}
    Priority: {}
    Parameters: {}'''.format(self.name,self.priority,self.parameter_dict)