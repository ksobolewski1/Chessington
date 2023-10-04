import Board
from Board import *
from MoveTree import *


# The Game class manages calls to Enginegton, operations on the move tree, and stores game data

class Game:
    def __init__(self, **kwargs):
        self.user_orientation = kwargs["pl_colour"]
        self.turn = kwargs["side_to_move"]
        self.from_position = kwargs["from_pos"]
        self.engine_on = kwargs["engine_on"]
        self.fen_dict = {0: "0", 1: "P", 2: "N", 3: "B", 5: "R", 9: "Q", 10: "K"}
        self.legal_moves = []
        self.engine_turn = -self.user_orientation
        self.engine_search_orientation = 0
        if kwargs["preexisting_tree"] is None:
            self.all_variations = MoveTree(**kwargs)
        else:
            self.all_variations = kwargs["preexisting_tree"]
        self.current_position = self.all_variations.root
        self.start_data = kwargs
        self.repetitions = {}

    def update(self, new_pos, enginegton):  # called on move_made() event
        events = [Event(50, board=0)]
        self.all_variations.add_move(new_pos, self.current_position, events)
        self.legal_moves.clear()
        if events[-1].ID == 1:
            return events
        self.current_position = new_pos
        self.turn = -self.turn

        self.get_legal_moves(events, enginegton)  # calls enginegton with "get"
        if events[0].data["board"] > 1:
            return events  # if game over, return here to avoid starting the engine

        fen = self.to_fen(self.current_position.position)
        if self.engine_on and self.turn == self.engine_turn:
            self.start_engine(enginegton, fen)  # calls enginegton with "find"

        if self.user_orientation == -1:  # check if fen needs to be adjusted for board flipping
            fen = fen[::-1]

        if fen in self.repetitions:  # check if a position has occurred using fen string as key
            self.repetitions[fen] += 1
            if self.repetitions[fen] >= 3 and self.engine_on and self.turn != self.engine_turn:
                events.append(Event(51))
        else:
            self.repetitions[fen] = 1

        return events

    def update_events(self, data, events):

        if data[3] == 1 and self.engine_on:
            events[0] = Event(50, board=3, winner="Insufficient material")
            return

        if data[0] == 1 and data[1] == 0:
            self.current_position.update_notation("+")
            events[0] = Event(50, board=1)
        elif data[1] == 1:
            self.current_position.update_notation("#")
            if self.engine_on and self.turn == 1:
                events[0] = Event(50, board=2, winner="Black wins")
            if self.engine_on and self.turn == -1:
                events[0] = Event(50, board=2, winner="White wins")
            if not self.engine_on:
                events[0] = Event(50, board=2)
        elif data[2] == 1:
            self.current_position.update_notation("=")
            if self.engine_on:
                events[0] = Event(50, board=3, winner="Stalemate")
            else:
                events[0] = Event(50, board=3)

    def get_engine_args(self, pref, fen):  # convert Game data for writing to enginegton_log.txt
        pref += fen + "\n"
        pref += str(self.n_val(self.turn)) + str(self.n_val(self.user_orientation)) + "\n"
        for i in self.current_position.move:
            pref += str(i)
        pref += "\n"
        pref += str(self.current_position.castling_rights)
        return pref

    @staticmethod
    def n_val(val):  # if value is -1, write "-" instead
        if val == -1:
            return "-"
        else:
            return str(val)

    def get_moves(self, enginegton):
        events = [Event(50, board=0)]
        self.legal_moves.clear()
        self.get_legal_moves(events, enginegton)
        return events

    def start_engine(self, enginegton, fen=None):
        if fen is None:
            fen = self.to_fen(self.current_position.position)
        self.engine_search_orientation = self.user_orientation
        self.to_fen(self.current_position.position)
        enginegton.post_message(self.get_engine_args("find\n", fen))
        enginegton.wait_for_ok()

    def get_legal_moves(self, events, enginegton):
        enginegton.post_message(self.get_engine_args("get\n", self.to_fen(self.current_position.position)))
        enginegton.wait_for_ok()
        self.update_events(self.parse_answer(enginegton.get_gr()), events)

    def parse_answer(self, res):  # convert engine's returned legal moves list
        for i in range(len(res)):
            if res[i] == "\n":
                return [int(res[i + 1]), int(res[i + 2]), int(res[i + 3]), int(res[i + 4])]
            if (i + 1) % 5 == 0:
                self.legal_moves.append([int(res[i - 4]), int(res[i - 3])] + [int(res[i - 2]), int(res[i - 1])])
                if 0 < int(res[i]) < 5:
                    self.legal_moves[-1].append(int(res[i]))
                else:
                    self.legal_moves[-1].append(0)

    def to_fen(self, pos):  # convert position to a fen string
        fen = ""
        for rank in pos:
            for file in rank:
                if file < 0:
                    fen += self.fen_dict[abs(file)].lower()
                else:
                    fen += self.fen_dict[file]
        return fen

    # Move tree and current_position operations

    def set_to_position(self, pos_id):  # search move tree dictionary to identify the new position to set to

        self.current_position = self.all_variations.key_search(pos_id)
        if self.current_position != self.all_variations.root:
            self.turn = self.current_position.turn * -1
        else:
            self.turn = self.current_position.turn
        self.all_variations.depth = self.current_position.ply
        if self.current_position.orientation != self.user_orientation:
            self.current_position.reverse_rank_and_file()
            Board.flip(self.current_position.position)

    def switch_position(self, direction):  # move up or down by 1 position

        if direction == -1 and self.current_position.parent is not None:
            self.current_position = self.current_position.parent
        elif direction == 1 and self.current_position.children:
            self.current_position = self.current_position.children[0]
        if self.current_position.orientation != self.user_orientation:
            self.current_position.reverse_rank_and_file()
            Board.flip(self.current_position.position)
        if self.current_position != self.all_variations.root:
            self.turn = self.current_position.turn * -1
        else:
            self.turn = self.current_position.turn

    def trim_tree(self):  # delete a subbranch starting at current position
        if self.current_position != self.all_variations.root:
            from_pos = self.current_position
            # removes all moves after from_pos inclusive
            self.set_to_position(from_pos.parent.key)
            self.current_position.children.remove(from_pos)

    def get_kwargs(self, engine_promotion=0):  # used by Chessington to relay game data to Board.update() functions
        kwargs = {"turn": self.turn, "pl_colour": self.user_orientation, "legal_moves": self.legal_moves,
                  "castle": self.current_position.castling_rights}
        if 0 < engine_promotion < 5:
            kwargs["engine_promo"] = engine_promotion
        return kwargs

    def get_draw_kwargs(self):  # used by Chessington to relay game data to Board.draw_pos()
        return {"move": self.current_position.move, "turn": self.turn, "orientation": self.user_orientation}
