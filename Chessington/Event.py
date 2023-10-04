class Event:
    def __init__(self, ID, **kwargs):
        self.ID = ID
        self.data = kwargs  # data to be passed to a corresponding event methods in Chessington
