import pygame
from Position import *
from Event import *
from Buttons import Button
from collections import Counter


class MoveGraphic:  # if the user requested engine move in Analysis, this will add circle shapes to mark the engine move
    def __init__(self, origin, destination, sqr_size):
        self.sqrs = (origin, destination)
        self.origin = (origin[1] * sqr_size + (sqr_size // 2), origin[0] * sqr_size + (sqr_size // 2))
        self.destination = (destination[1] * sqr_size + (sqr_size // 2), destination[0] * sqr_size + (sqr_size // 2))
        self.radius = sqr_size // 2
        self.width = sqr_size // 10

    def draw(self, window_data):
        pygame.draw.circle(window_data.window, (127, 23, 52), self.origin, self.radius, width=self.width)
        pygame.draw.circle(window_data.window, (127, 23, 52), self.destination, self.radius, width=self.width)

    def sqrs_to_list(self):  # convert sqrs tuple to list
        return list(self.sqrs[0]) + list(self.sqrs[1])


# The Board instance draws the position and the material difference,
# manages piece moving (its user interface and visuals), and processes a move (spawning the new Position instance,
# defining the chess notation for the move)
class Board:
    def __init__(self, graphics, board):
        self.position = []
        self.editor_position = []
        self.promotion_dict = {1: 2, 2: 3, 3: 5, 4: 9}
        self.file_dict = {0: "a", 1: "b", 2: "c", 3: "d", 4: "e", 5: "f", 6: "g", 7: "h"}
        self.piece_dict = {2: "N", 3: "B", 5: "R", 9: "Q", 10: "K", -2: "N", -3: "B", -5: "R", -9: "Q", -10: "K"}
        self.piece_count = {}
        self.selected_piece = None
        self.origin_square = None
        self.dragging = False
        self.sqr_under_mouse = None
        self.move_graphic = None
        self.piece_graphics = graphics
        self.min_piece_graphics = {key: pygame.transform.scale(
            graphic, (graphic.get_width() * 0.6, graphic.get_height() * 0.6)) for key, graphic in graphics.items()}
        self.board_graphic = board
        self.board_situation = 0  # designates check, stalemate or checkmate : 1: check, 2: checkmate, 3: stalemate
        self.captured = []

    def game_update(self, window_data, user_input, **kwargs):

        events = []
        mouse_rank = user_input.position[1] // window_data.sqr_size
        mouse_file = user_input.position[0] // window_data.sqr_size

        cursor_over = 0 <= mouse_rank < 8 and 0 <= mouse_file < 8

        if cursor_over:
            if user_input.left_click and not self.dragging and self.position[mouse_rank][mouse_file] != 0 \
                    and self.valid_piece(self.position[mouse_rank][mouse_file], kwargs["turn"], kwargs["engine"]):
                self.selected_piece = self.position[mouse_rank][mouse_file]
                self.origin_square = (mouse_rank, mouse_file)

            if not user_input.left_click and self.selected_piece is not None and self.dragging:
                if self.valid_move(kwargs["legal_moves"], self.origin_square, (mouse_rank, mouse_file)):
                    pos = self.process_move(self.selected_piece, self.origin_square, (mouse_rank, mouse_file), **kwargs)
                    self.reset_board_UI()
                    events.append(Event(43, position=pos))
                else:
                    self.piece_back()
                self.dragging = False
            self.sqr_under_mouse = (mouse_rank, mouse_file) if self.selected_piece is not None \
                                                               and self.valid_move(kwargs["legal_moves"],
                                                                                   self.origin_square,
                                                                                   (mouse_rank, mouse_file)) else None
        else:
            self.sqr_under_mouse = None
            self.piece_back()

        if user_input.right_click and self.selected_piece is not None:
            self.piece_back()
            self.reset_board_UI()

        if user_input.left_click and self.selected_piece is not None and cursor_over:
            window_data.window.blit(self.piece_graphics[self.selected_piece],
                                    (user_input.position[0] - window_data.sqr_size // 2,
                                     user_input.position[1] - window_data.sqr_size // 2))
            self.position[self.origin_square[0]][self.origin_square[1]] = 0
            self.dragging = True

        return events

    def editor_update(self, window_data, user_input):

        mouse_rank = user_input.position[1] // window_data.sqr_size
        mouse_file = user_input.position[0] // window_data.sqr_size

        cursor_over = 0 <= mouse_rank < 8 and 0 <= mouse_file < 8

        if user_input.left_click and self.selected_piece is not None:
            window_data.window.blit(self.piece_graphics[self.selected_piece],
                                    (user_input.position[0] - window_data.sqr_size // 2,
                                     user_input.position[1] - window_data.sqr_size // 2))
            self.dragging = True

        edited = False
        if not user_input.left_click and self.selected_piece is not None and self.dragging:
            if cursor_over and not ((self.selected_piece == 1 or self.selected_piece == -1)
                                    and (mouse_rank == 0 or mouse_rank == 7)):
                self.position[mouse_rank][mouse_file] = self.selected_piece
                edited = True
            self.dragging = False

        if cursor_over and self.selected_piece is not None and not (
                (self.selected_piece == 1 or self.selected_piece == -1)
                and (mouse_rank == 0 or mouse_rank == 7)):
            self.sqr_under_mouse = (mouse_rank, mouse_file)
        else:
            self.sqr_under_mouse = None

        if user_input.right_click:
            if 0 < user_input.position[0] < window_data.width - window_data.side_margin_size:
                self.position[mouse_rank][mouse_file] = 0
            self.reset_board_UI()

        if edited:
            self.reset_board_UI()
        return [Event(7, edited=edited)]

    def process_move(self, piece, origin, destination, **kwargs):

        orient = kwargs["pl_colour"]
        notation = self.get_notation(piece, origin, destination, **kwargs)

        cap_piece = self.position[destination[0]][destination[1]]
        captured = []

        if cap_piece == 9 or cap_piece == -9:
            captured.append(1 * -kwargs["turn"]) if self.piece_count[cap_piece] > 1 else captured.append(cap_piece)
            self.piece_count[cap_piece] -= 1
        elif 2 <= abs(cap_piece) <= 5:
            captured.append(1 * -kwargs["turn"]) if self.piece_count[cap_piece] > 2 else captured.append(cap_piece)
            self.piece_count[cap_piece] -= 1
        elif cap_piece != 0:
            captured.append(cap_piece)

        self.position[origin[0]][origin[1]] = 0

        if (piece == 10 or piece == -10) and (abs(origin[1] - destination[1]) > 1):
            notation = self.castle((origin[1] == 4 and orient == 1 and destination[1] > origin[1])
                                   or (origin[1] == 3 and orient == -1 and destination[1] < origin[1]),
                                   piece, origin, orient)

        elif (piece == 1 or piece == -1) and destination[1] != origin[1] \
                and cap_piece == 0:
            captured.append(-piece)
            self.en_passant(origin, destination)

        # Updating castling rights; this has been the most error-prone part of this method's code,
        # so I lay out the logic of this update as much as possible
        black_c = kwargs["castle"] % 4
        white_c = (kwargs["castle"] - black_c) >> 2
        # at the end of this function, the Position constructor is passed castling rights' value as int in range 0-15,
        # corresponding to all 16 possible combinations of castling rights of both sides

        if white_c > 0:
            if (piece == 5 and origin[1] == 7 and orient == 1) or (piece == 5 and origin[1] == 0 and orient == -1):
                white_c -= 2
            if (piece == 5 and origin[1] == 0 and orient == 1) or (piece == 5 and origin[1] == 7 and orient == -1):
                white_c -= 1
            if piece == 10:
                white_c = 0

        if black_c > 0:
            if (piece == -5 and origin[1] == 7 and orient == 1) or (piece == -5 and origin[1] == 0 and orient == -1):
                black_c -= 2
            if (piece == -5 and origin[1] == 0 and orient == 1) or (piece == -5 and origin[1] == 7 and orient == -1):
                black_c -= 1
            if piece == -10:
                black_c = 0

        if cap_piece == 5 and white_c > 0:
            if (destination[1] == 7 and orient == 1) or (destination[1] == 0 and orient == -1):
                white_c -= 2
            elif (destination[1] == 0 and orient == 1) or (destination[1] == 7 and orient == -1):
                white_c -= 1
        elif cap_piece == -5 and black_c > 0:
            if (destination[1] == 7 and orient == 1) or (destination[1] == 0 and orient == -1):
                black_c -= 2
            elif (destination[1] == 0 and orient == 1) or (destination[1] == 7 and orient == -1):
                black_c -= 1

        if white_c < 0:
            white_c = 0
        if black_c < 0:
            black_c = 0

        if "engine_promo" in kwargs:
            piece = self.promotion_dict[kwargs["engine_promo"]] * kwargs["turn"]
            notation += "=" + self.piece_dict[piece]

        self.position[destination[0]][destination[1]] = piece

        return Position(side_to_move=kwargs["turn"], pl_colour=orient, position=self.copy_board(self.position),
                        move=[origin[0], origin[1], destination[0], destination[1]], move_note=notation,
                        castle=(white_c << 2) + black_c,
                        promotion=(piece == 1 or piece == -1) and (destination[0] == 0 or destination[0] == 7),
                        material=captured)

    def castle(self, short, piece, origin, player_colour):

        notation = "0-0" if short else "0-0-0"
        if (short and player_colour == 1) or (not short and player_colour == -1):
            self.position[origin[0]][origin[1] + 1] = 50 // piece
            self.position[origin[0]][7] = 0
        elif (short and player_colour == -1) or (not short and player_colour == 1):
            self.position[origin[0]][origin[1] - 1] = 50 // piece
            self.position[origin[0]][0] = 0
        return notation

    def en_passant(self, origin, destination):
        if destination[1] > origin[1]:
            self.position[origin[0]][origin[1] + 1] = 0
        else:
            self.position[origin[0]][origin[1] - 1] = 0

    def get_notation(self, piece, origin, destination, **kwargs):

        notation = ""
        pawn_file = ""
        if piece != 1 and piece != -1:
            notation += self.piece_dict[piece] + self.notation_ambiguity(piece, origin, destination, **kwargs)
        else:
            pawn_file = self.file_dict[origin[1]]
        if self.position[destination[0]][destination[1]] != 0 \
                or ((piece == 1 or piece == -1) and destination[1] != origin[1]
                    and self.position[destination[0]][destination[1]] == 0):
            if piece == 1 or piece == -1:
                notation += pawn_file + "x"
            else:
                notation += "x"
        if kwargs["pl_colour"] == 1:
            notation += self.file_dict[destination[1]] + str(9 - (destination[0] + 1))
        else:
            notation += self.file_dict[destination[1]] + str((destination[0] + 1))
        return notation

    @staticmethod
    def valid_move(legal_moves, square, destination_sqr):
        if square == destination_sqr:
            return False
        for move in legal_moves:
            if move[0] == square[0] and move[1] == square[1] and move[2] == destination_sqr[0] \
                    and move[3] == destination_sqr[1]:
                return True
        return False

    @staticmethod
    def valid_piece(piece, turn, engine):
        if not engine:
            return (piece > 0 and turn == 1) or (piece < 0 and turn == -1)
        else:
            return False  # if the engine is searching, no piece can be selected

    # if a piece of the same id could have moved to the same square, the method below resolves the ambiguity
    def notation_ambiguity(self, piece, origin, destination, **kwargs):
        for move in kwargs["legal_moves"]:
            if move[2] == destination[0] and move[3] == destination[1] \
                    and self.position[move[0]][move[1]] == piece:
                if origin[1] != move[1]:
                    return self.file_dict[origin[1]]
                if origin[0] != move[0]:
                    txt = str(9 - (origin[0] + 1)) if kwargs["pl_colour"] == 1 else str((origin[0] + 1))
                    return txt
        return ""

    def clear_pos(self):
        self.position = [[0 for _ in range(8)] for _ in range(8)]

    def is_empty(self):
        return not any(self.position[rank][file] != 0 for rank in range(8) for file in range(8))

    def flip_file_notation(self, sqr_size):
        new = {}
        for key, value in self.file_dict.items():
            new[abs(key - 7)] = value
        self.file_dict = new
        if self.move_graphic is not None:
            move = self.move_graphic.sqrs_to_list()
            Position.reverse_move(move)
            self.move_graphic = MoveGraphic((move[0], move[1]), (move[2], move[3]), sqr_size)

    def draw_pos(self, window_data, **kwargs):  # draws the board

        w = window_data.sqr_size
        font = pygame.font.SysFont("Futura", w // 4)
        turn = 1 if "turn" not in kwargs else kwargs["turn"]
        window_data.window.blit(self.board_graphic, (0, 0))
        self.draw_material_imbalance(window_data)

        for rank in range(8):
            for file in range(8):
                if self.board_situation > 0:
                    # draw check
                    if (self.position[rank][file] == 10 and turn == 1 and self.board_situation == 1) \
                            or (self.position[rank][file] == -10 and turn == -1 and self.board_situation == 1):
                        pygame.draw.rect(window_data.window, (127, 23, 52), pygame.Rect(file * w, rank * w, w, w), 6)
                    # draw checkmate
                    if (self.position[rank][file] == 10 and turn == 1 and self.board_situation == 2) \
                            or (self.position[rank][file] == -10 and turn == -1 and self.board_situation == 2):
                        pygame.draw.rect(window_data.window, (127, 23, 52), pygame.Rect(file * w, rank * w, w, w))
                    # draw stalemate
                    if ((self.position[rank][file] == 10 or self.position[rank][file] == -10)
                            and self.board_situation == 3):
                        pygame.draw.rect(window_data.window, (35, 35, 35), pygame.Rect(file * w, rank * w, w, w))

                if self.position[rank][file] != 0:
                    window_data.window.blit(self.piece_graphics[self.position[rank][file]], (file * w, rank * w))

                # draw notation on the board:
                orientation = 1 if "orientation" not in kwargs else kwargs["orientation"]
                if rank == 7:
                    x_pos = file * w if orientation == 1 else file * w + w - font.size(self.file_dict[file])[0]
                    Button.draw_text(window_data, self.file_dict[file], font, (0, 0, 0), x_pos,
                                     rank * w + w - font.size(self.file_dict[file])[1])
                if (file == 0 and orientation == 1) or (file == 7 and orientation == -1):
                    x_pos = file * w if orientation == 1 else file * w + w - font.size(str(8 - rank))[0]
                    order = 8 - rank if orientation == 1 else rank + 1
                    Button.draw_text(window_data, str(order), font, (0, 0, 0), x_pos, rank * w)

                if "move" in kwargs:
                    if kwargs["move"]:
                        if rank == kwargs["move"][0] and file == kwargs["move"][1]:
                            pygame.draw.rect(window_data.window, (0, 0, 0), pygame.Rect(file * w, rank * w, w, w), 1)
                        if rank == kwargs["move"][2] and file == kwargs["move"][3]:
                            pygame.draw.rect(window_data.window, (0, 0, 0), pygame.Rect(file * w, rank * w, w, w), 2)

                if self.sqr_under_mouse is None or self.selected_piece is None:
                    continue
                if (rank, file) == self.sqr_under_mouse:
                    if self.origin_square is None:
                        pygame.draw.rect(window_data.window, (0, 0, 0), pygame.Rect(file * w, rank * w, w, w), 3)
                    elif self.position[rank][file] == 0 \
                            and not (abs(self.selected_piece == 1) and file != self.origin_square[1]):
                        pygame.draw.rect(window_data.window, (0, 0, 0), pygame.Rect(file * w, rank * w, w, w), 3)
                    else:
                        pygame.draw.rect(window_data.window, (127, 23, 52), pygame.Rect(file * w, rank * w, w, w), 3)

        if self.move_graphic is not None:
            self.move_graphic.draw(window_data)

    def draw_material_imbalance(self, window_data):

        inc = window_data.sqr_size // 4
        start_w = window_data.width - window_data.side_margin_size + window_data.sqr_size * 0.2
        hb = window_data.height - window_data.sqr_size
        ww = 0
        wb = 0
        for piece in self.captured:
            if piece > 0:
                window_data.window.blit(self.min_piece_graphics[piece], (start_w + ww, hb - window_data.sqr_size * 0.5))
                ww += inc
            else:
                window_data.window.blit(self.min_piece_graphics[piece], (start_w + wb, hb))
                wb += inc

    def piece_back(self):  # puts the piece back if it was dropped illegally (on invalid square or out of board bounds)
        if self.selected_piece is not None:
            self.position[self.origin_square[0]][self.origin_square[1]] = self.selected_piece

    def reset_board_UI(self, res_sit=False):
        self.selected_piece = None
        self.origin_square = None
        self.dragging = False
        self.sqr_under_mouse = None
        self.move_graphic = None
        if res_sit:
            self.board_situation = 0
            self.captured.clear()

    def set_board(self, pos):
        self.set_pos(pos.position)
        self.captured = pos.captured

    def set_pos(self, pos):
        self.position = self.copy_board(pos)
        counts = Counter(element for row in self.position for element in row)
        self.piece_count = {key: counts.get(key, 0) for key in [2, -2, 3, -3, 5, -5, 9, -9]}

    def redraw(self, graphics, board, sqr_size):  # called when application resizes
        self.piece_graphics = graphics
        self.min_piece_graphics = {key: pygame.transform.scale(
            graphic, (graphic.get_width() * 0.6, graphic.get_height() * 0.6)) for key, graphic in graphics.items()}
        self.board_graphic = board
        if self.move_graphic is not None:
            self.move_graphic = MoveGraphic(self.move_graphic.sqrs[0], self.move_graphic.sqrs[1], sqr_size)

    @staticmethod
    def flip(pos):
        for rank in range(8):
            pos[rank].reverse()
        pos.reverse()

    @staticmethod
    def start_pos():
        return [
            [-5, -2, -3, -9, -10, -3, -2, -5],
            [-1, -1, -1, -1, -1, -1, -1, -1],
            [0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0],
            [1, 1, 1, 1, 1, 1, 1, 1],
            [5, 2, 3, 9, 10, 3, 2, 5]
        ]

    @staticmethod
    def copy_board(board):
        return [rank.copy() for rank in board]
