from Position import *
from Event import *


class MoveTree:
    def __init__(self, **kwargs):
        self.root = Position(key="r", material=[], **kwargs)
        # the maximum number of children a position can lead to
        self.branch_limit = 5
        # the current branch depth
        self.position_lookup = {"r": self.root}
        self.branch_limit = 5  # controls how many branches can be started from one position
        self.tree_height = 0

    def key_search(self, key):
        return self.position_lookup[key]

    def add_move(self, new_pos, current_pos, events):

        new_pos.ply = current_pos.ply
        if new_pos.turn == 1:
            new_pos.ply += 1
            new_pos.update_notation(str(new_pos.ply) + ". ", True)
        if new_pos.turn == -1 and current_pos == self.root:
            new_pos.update_notation("... ", True)

        if len(current_pos.children) >= self.branch_limit:
            events.append(Event(1, direction=-1))  # if branching limit is crossed, the app will take that move back
            return
        callback = []
        if self.pos_in_tree(current_pos.children, new_pos.move_notation, callback) and current_pos.children:
            events.append(Event(0, position_key=current_pos.children[callback[0]].key))
            return  # if the position is already in the tree, spawn a takeback(1) event
        if self.tree_height == self.branch_limit and current_pos.children:
            events.append(Event(0, position_key=current_pos.children[0].key))
            return  # if branch limit is exceeded, spawn a takeback(-1) event and do not add the position into the tree

        new_pos.captured = new_pos.captured + current_pos.captured  # increment the captured pieces list
        new_pos.key = id(new_pos)  # generate the id from Position instance id
        current_pos.children.append(new_pos)
        new_pos.parent = current_pos
        self.position_lookup[new_pos.key] = new_pos

    # part tree
    def get_part_tree(self, key, part_tree):
        position = self.position_lookup[key]
        position.tree_backtrack(part_tree, False)
        self.tree_height = max(part_tree.keys())
        if position.children:
            position.children[0].follow_line(part_tree, self.tree_height)
        return part_tree

    @staticmethod
    def pos_in_tree(siblings, new_pos_notation, callback):
        # this function compares moves by notation - in case the existing position ends with check, checkmate or
        # stalemate symbols, these symbols are added for the comparison since the incoming position does not have
        # them yet: its notation is updated later on in Game.update(), when Enginegton is called
        # to define the legal moves for the incoming position, and one of these situations is detected.
        for index, sibling in enumerate(siblings):
            if sibling.move_notation[-1] == "+" or sibling.move_notation[-1] == "=" or sibling.move_notation[-1] == "#":
                if new_pos_notation + sibling.move_notation[-1] == sibling.move_notation:
                    callback.append(index)
                    return True
            if sibling.move_notation == new_pos_notation:
                callback.append(index)
                return True
        return False
