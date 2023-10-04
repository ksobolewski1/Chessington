class Position:
    def __init__(self, **kwargs):
        self.turn = kwargs["side_to_move"]   # which side moved to bring this position about
        self.orientation = kwargs["pl_colour"]
        self.move = kwargs["move"]  # move in list format
        self.move_notation = kwargs["move_note"]  # move in chess notation format
        self.children = []
        self.parent = None
        if kwargs["promotion"]:
            setattr(self, "promotion", True)
        self.ply = 0  # a ply is white's move + black's move
        self.position = kwargs["position"]
        self.castling_rights = kwargs["castle"]
        if "key" in kwargs:
            self.key = kwargs["key"]  # if key in kwargs, this is the root of the MoveTree
        else:
            self.key = 0  # will be obtained from id(self) when the position is added to the tree
        self.captured = kwargs["material"]

    def __eq__(self, other):
        if not isinstance(other, Position):
            return False
        return self.key == other.key

    # this function reconstructs a part tree (all the positions to be displayed by the variations UI)
    # by moving backwards through Position parents until it hits the root
    # if the position has siblings, it means branching was encountered:
    # the keys need to be incremented and new 0-depth added to the tree_dict,
    # and, the alternative line needs to be followed from this junction to get the rest of it for drawing
    def tree_backtrack(self, tree_dict, line_cont):
        # line_cont bool is set if a branching needs to be followed before the backtrack can proceed
        tree_dict[0].insert(0, self)
        if line_cont and self.children:
            self.children[0].follow_line(tree_dict, 0)

        if self.key == "r":  # root is hit
            return

        # entering a lower branch, one depth lower
        if self.has_siblings() and self.get_sibling_index() > 0:
            # increment keys and add new depth 0
            max_key = max(tree_dict.keys())
            for i in range(len(tree_dict)):
                tree_dict[max_key + 1] = tree_dict[max_key]
                tree_dict.pop(max_key)
                max_key -= 1
            tree_dict[0] = []
            self.parent.tree_backtrack(tree_dict, True)  # set line_cont to true to follow the alternative
        else:
            self.parent.tree_backtrack(tree_dict, False)

    def follow_line(self, tree_dict, max_key):
        tree_dict[max_key].append(self)
        if self.children:
            self.children[0].follow_line(tree_dict, max_key)

    # this function is used in every takeback scenario,
    # if this position's orientation is not equal to the current orientation, its data is "flipped" too
    def reverse_rank_and_file(self):
        if len(self.move) > 0 and self.move[0] != -1:
            self.reverse_move(self.move)
        self.orientation *= -1

    def update_notation(self, add, prefix=False):
        if len(self.move_notation) == 0:
            return
        if self.move_notation[-1] != add and not prefix:
            self.move_notation += add
        elif prefix:
            self.move_notation = add + self.move_notation

    def has_siblings(self):
        if self.key == "r":
            return False
        return len(self.parent.children) > 1

    def get_sibling_index(self):
        for index, sibling in enumerate(self.parent.children):
            if sibling == self:
                return index

    def find_branch_ancestor(self):
        if self.get_sibling_index() != 0:
            return self
        return self.parent.find_branch_ancestor()

    @staticmethod
    def get_next(current):
        if len(current.children) == 0:
            return None
        return current.children[0]

    @staticmethod
    def reverse_move(move):
        move[:] = [7 - m if i != 4 else move[-1] for i, m in enumerate(move)]
