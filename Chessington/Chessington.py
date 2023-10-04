from UI_Types import *
from Game import *
import ctypes
import sys
import multiprocessing


# Wrapper class for mouse presses and position, and arrow keys
class Input:
    def __init__(self, pos, left, right, dragging):
        self.position = pos
        self.left_click = left
        self.right_click = right
        self.arrow_press = 0
        self.dragging = dragging
        self.wheel = None

    def update(self, dragging, wheel):
        self.position = pygame.mouse.get_pos()
        self.left_click = pygame.mouse.get_pressed()[0]
        self.right_click = pygame.mouse.get_pressed()[2]
        self.dragging = dragging
        self.wheel = wheel
        k_press = pygame.key.get_pressed()
        self.arrow_press = -1 if k_press[pygame.K_LEFT] else 1 if k_press[pygame.K_RIGHT] else 0

    def any_click(self):
        return self.left_click or self.right_click


# Wrapper class for pygame window and app dimensions
class WindowData:
    def __init__(self, dimensions):
        self.width = dimensions[0]
        self.height = dimensions[1]
        self.sqr_size = dimensions[2]
        self.side_margin_size = dimensions[3]
        self.window = pygame.display.set_mode((self.width, self.height), pygame.RESIZABLE)

    def set_new_dimensions(self, dimensions):
        self.width = dimensions[0]
        self.height = dimensions[1]
        self.sqr_size = dimensions[2]
        self.side_margin_size = dimensions[3]


# Enginegton dll with ctypes
if getattr(sys, 'frozen', False):
    base_dir = os.path.dirname(sys.executable)
else:
    base_dir = os.path.dirname(os.path.abspath(__file__))
eng_path = os.path.join(base_dir, "Enginegton2", "Enginegton2.dll")
os.add_dll_directory(os.path.dirname(eng_path))  # for Windows; remove for macOS
Enginegton = ctypes.cdll.LoadLibrary(eng_path)
RunEngine = Enginegton.Run
RunEngine.argtypes = [ctypes.c_char_p]


# Interface for communicating with the Enginegton dll/dylib
class Enginegton:
    def __init__(self, window_data, path, p_path):
        self.path_to_log = path
        log = open(self.path_to_log, "w")
        log.truncate(0)
        log.close()
        self.engine_process = multiprocessing.Process(target=self.startup, args=(path.encode(), p_path.encode(),))
        self.engine_process.start()
        self.icon = pygame.transform.scale(pygame.image.load(os.path.join("assets", "img", "icon.png")),
                                           (window_data.sqr_size * 2, window_data.sqr_size * 2))
        self.icon_rot = 0
        self.searching = False

    @staticmethod
    def startup(path, private):
        RunEngine(path, private)

    def post_message(self, message):
        log = open(self.path_to_log, "w")
        log.write(message)
        log.close()
        self.searching = True if message[0] == "f" else False

    def wait_for_ok(self):
        while True:
            log = open(self.path_to_log, "r+")
            if log.read() == "ok":
                log.truncate(0)
                break
            log.close()

    # "get" result - reads out the result of "get" request
    def get_gr(self):
        rem = None
        while rem is None:
            log = open(self.path_to_log, "r")
            if log.read(1) == "/":
                rem = log.read()
            log.close()
        return rem

    # "find" result - reads out the result of "find" request
    def get_fr(self):
        if not self.searching:
            return ""
        log = open(self.path_to_log, "r")
        rem = log.read()
        log.close()
        return rem

    def run_animation(self, window_data, default=True):
        self.icon_rot += 1
        if default:
            rect = self.icon.get_rect(center=(window_data.sqr_size * 8 + window_data.sqr_size // 4,
                                              window_data.sqr_size // 2))
        else:
            rect = self.icon.get_rect(center=(window_data.width // 2 - window_data.sqr_size * 2,
                                              window_data.height // 2))
        icon = pygame.transform.rotozoom(self.icon.copy(), self.icon_rot, 1)
        new_rect = icon.get_rect(center=rect.center)
        window_data.window.blit(icon, new_rect)


class Chessington:
    def __init__(self):
        self.running = True
        self.clock = pygame.time.Clock()
        self.scene = -1
        self.scenes = {-1: self.loading_screen, 0: self.update_menu, 1: self.update_game, 2: self.update_editor}
        self.window_data = WindowData(self.get_dimensions(self.get_default_height()))
        self.input = Input(pygame.mouse.get_pos(), pygame.mouse.get_pressed()[0], pygame.mouse.get_pressed()[2], False)
        pygame.display.set_caption("Chessington")
        self.current_UI = None
        self.board = None
        self.current_game = None
        self.events = {0: self.take_to_position, 1: self.take_back, 2: self.delete_from_here, 3: self.promote_variation,
                       4: self.engine_recommendation, -4: self.stop_engine, 5: self.get_new_piece,
                       6: self.clear_editor_selection, 7: self.reset_editor_piece_buttons, 8: self.switch_branch,
                       -8: self.minimise_branch, 21: self.quit_app, 22: self.to_menu, 23: self.to_editor,
                       24: self.to_game, 33: self.restart_game, 40: self.reset_editor, 41: self.flip_board,
                       42: self.btn_take_back, 43: self.move_made, 44: self.process_promotion, 50: self.game_event,
                       51: self.enable_draw_claim}
        self.sounds = {2: pygame.mixer.Sound(os.path.join("assets", "sounds", "check.wav")),
                       1: pygame.mixer.Sound(os.path.join("assets", "sounds", "norm.wav")),
                       0: pygame.mixer.Sound(os.path.join("assets", "sounds", "over.wav"))}
        self.enginegton = Enginegton(self.window_data, base_dir + "\\enginegton_log.txt",
                                     base_dir + "\\Enginegton2\\search_log.txt")

    def update_app(self, mouse_wheel):
        if self.board is not None:
            self.input.update(self.board.dragging, mouse_wheel)
        self.scenes[self.scene]()

    def quit_app(self):
        self.running = False
        self.terminate_engine()

    def loading_screen(self):  # Wait for Enginegton to initialise
        eng_ready = False
        self.window_data.window.fill((255, 255, 255), (0, 0, pygame.display.get_surface().get_size()[0],
                                                       pygame.display.get_surface().get_size()[1]))
        self.enginegton.run_animation(self.window_data, False)
        Button.draw_text(self.window_data, "Loading Enginegton",
                         pygame.font.SysFont("Futura", self.window_data.sqr_size // 2), (0, 0, 0),
                         self.window_data.width // 2 - self.window_data.sqr_size,
                         self.window_data.height // 2)

        log = open(self.enginegton.path_to_log, "r+")
        if log.read() == "ok":
            eng_ready = True
        log.close()

        if eng_ready:
            self.enginegton.icon = pygame.transform.scale(pygame.image.load(os.path.join("assets", "img", "icon.png")),
                                                          (self.window_data.sqr_size // 2,
                                                           self.window_data.sqr_size // 2))
            self.board = Board(self.load_all_pieces(), self.load_board_graphic())
            self.to_menu()

    def to_menu(self):
        self.stop_engine()
        time.sleep(0.1)
        self.scene = 0
        self.current_UI = GeneralUI(GeneralUI.get_menu_UI, win_data=self.window_data, graphics=self.load_menu())
        self.board.editor_position.clear()

    def to_game(self, **kwargs):

        if kwargs["from_pos"] and self.king_count() != (1, 1):
            return

        time.sleep(0.1)
        self.scene = 1
        if kwargs["from_pos"]:
            # Store the editor position in case the user wants to go back to the editor
            self.board.editor_position = Board.copy_board(self.board.position)
            castle = self.verify_castling_choices(self.current_UI.sub_interface.get_castling_choices())
        else:
            self.board.set_pos(self.board.start_pos())
            castle = 15

        if kwargs["pl_colour"] == -1:
            for rank in range(8):
                self.board.position[rank].reverse()
            self.board.position.reverse()
            self.board.flip_file_notation(self.window_data.sqr_size)

        self.current_UI = GeneralUI(GeneralUI.get_game_UI, win_data=self.window_data, engine_on=kwargs["engine_on"])
        if kwargs["from_pos"]:
            self.current_UI.dropdowns[0].buttons.append(self.current_UI.add_special_button(self.window_data, "editor"))
        self.board.reset_board_UI(True)

        self.current_game = Game(position=Board.copy_board(self.board.position), move=[-1, -1, -1, -1], move_note="",
                                 castle=castle, promotion=False, enginegton=self.enginegton, **kwargs)
        self.handle_events(self.current_game.get_moves(self.enginegton))
        if self.current_game.engine_on and self.current_game.turn == self.current_game.engine_turn:
            self.current_game.start_engine(self.enginegton)

    def to_editor(self):
        if self.current_game is not None:
            self.stop_engine()
        time.sleep(0.1)
        self.scene = 2
        self.current_UI = GeneralUI(GeneralUI.get_editor_UI, win_data=self.window_data, graphics=self.load_all_pieces())
        self.board.reset_board_UI(True)

        if not self.board.editor_position:
            self.board.set_pos(self.board.start_pos())
            self.board.clear_pos()
        else:
            self.board.set_pos(self.board.editor_position)
            self.board.editor_position.clear()

    def update_menu(self):
        self.handle_events(self.current_UI.update(self.window_data, self.input))

    def update_game(self):
        self.handle_events(self.current_UI.update(self.window_data, self.input, engine=self.current_game.engine_on))
        self.board.draw_pos(self.window_data, **self.current_game.get_draw_kwargs())
        if self.scene != 1:
            return
        self.handle_events(self.board.game_update(self.window_data, self.input,
                                                  engine=self.current_game.engine_on and self.enginegton.searching,
                                                  **self.current_game.get_kwargs()))

        self.process_engine_move(self.enginegton.get_fr())
        if self.enginegton.searching:
            self.enginegton.run_animation(self.window_data)

    def update_editor(self):
        self.handle_events(self.current_UI.update(self.window_data, self.input))
        self.board.draw_pos(self.window_data)
        if self.scene != 2:
            return
        self.handle_events(self.board.editor_update(self.window_data, self.input))
        if not self.board.is_empty() and sum(1 for btn in self.current_UI.buttons if btn.event_ID == 40) == 0:
            self.current_UI.buttons.append(self.current_UI.add_special_button(self.window_data, "reset"))
        if self.board.is_empty() and sum(1 for btn in self.current_UI.buttons if btn.event_ID == 40) > 0:
            self.current_UI.buttons = list(filter(lambda btn: btn.event_ID != 40, self.current_UI.buttons))

    # Every event ID is passed down in lists from the different UIs, Game, or Board instances, and processed below
    def handle_events(self, event_list):
        for event in event_list:
            if event.ID not in self.events:
                continue
            try:
                # noinspection PyArgumentList
                self.events[event.ID](**event.data)
            except KeyError:
                pass

    def process_promotion(self, **kwargs):  # if a piece was selected to promote to
        position_data = self.current_UI.sub_interface.position_data
        if self.current_game.user_orientation != position_data.orientation:
            position_data.reverse_rank_and_file()
            Board.flip(position_data.position)
        self.board.position[position_data.move[2]][position_data.move[3]] = kwargs["id"]
        self.board.piece_count[kwargs["id"]] += 1
        position_data.position = Board.copy_board(self.board.position)
        position_data.update_notation("=" + self.board.piece_dict[kwargs["id"]])
        delattr(position_data, "promotion")
        self.current_UI.get_variations_UI(win_data=self.window_data, engine_on=self.current_game.engine_on)
        self.move_made(position=position_data)

    def move_made(self, **kwargs):
        if self.enginegton.searching:  # if the user requested an engine move in analysis but then makes a move
            self.stop_engine()
        position = kwargs["position"]
        # if hasattr promotion, this function returns - once the player chooses a piece, process_promotion(), def above,
        # calls it again with the same position but without promotion attr, to finish processing a move
        if hasattr(position, "promotion"):
            self.current_UI.get_promotion_UI(win_data=self.window_data, graphics=self.load_pieces(), position=position)
            return
        self.current_UI.remove_special_button("claim")
        self.handle_events(self.current_game.update(position, self.enginegton))
        if self.board.board_situation == 1:
            pygame.mixer.Sound.play(self.sounds[1])
        else:
            pygame.mixer.Sound.play(self.sounds[2])
        self.update_variations(self.current_game.current_position, type=1)
        self.board.captured = self.current_game.current_position.captured

    def update_variations(self, position, **kwargs):

        if self.current_UI.sub_interface is None:
            return

        part_tree = {0: []}  # part_tree represents the currently visible (drawable) part of the tree
        self.current_game.all_variations.get_part_tree(position.key, part_tree)
        tree_height = self.current_game.all_variations.tree_height
        for depth, line in part_tree.items():
            self.current_UI.sub_interface.get_notation_buttons(self.window_data, line, depth, position.key)
            self.current_UI.sub_interface.update_scrolling_height(depth)

        self.current_UI.sub_interface.no_branch(tree_height)
        if position.has_siblings():
            index = 1 if position.get_sibling_index() > 0 else 0
            self.current_UI.sub_interface.get_branch_buttons(self.window_data, position.parent.children[1:],
                                                             tree_height - index, position.key)
        if "branches_redraw" in kwargs:
            self.current_UI.sub_interface.get_branch_buttons(self.window_data,
                                                             kwargs["branches_redraw"].parent.children[1:],
                                                             tree_height, kwargs["branches_redraw"].key)

        if position.key == "r":
            self.current_UI.sub_interface.no_modify()
            return
        if tree_height == 0:
            self.current_UI.sub_interface.allow_delete(self.window_data)
        else:
            self.current_UI.sub_interface.allow_branch_promotions(self.window_data)

        if kwargs["type"] == 1 and self.current_game.current_position.ply > 1:
            self.current_UI.sub_interface.scrollers[tree_height].forward_offset(-self.current_game.turn)
        elif kwargs["type"] == -1:
            self.current_UI.sub_interface.scrollers[tree_height].back_offset(-self.current_game.turn)
        elif kwargs["type"] == 0:
            self.current_UI.sub_interface.scrollers[tree_height].reset()
        else:
            self.current_UI.sub_interface.scrollers[tree_height].reset_to_max()

    def engine_recommendation(self):
        if self.board.board_situation > 1 or self.enginegton.searching:
            return
        self.current_game.start_engine(self.enginegton)

    def terminate_engine(self):  # end the engine process
        self.enginegton.post_message("term\n")
        self.enginegton.wait_for_ok()
        self.enginegton.engine_process.join()

    def stop_engine(self):  # stop the engine if it is searching
        if self.current_game is None:
            return
        self.enginegton.post_message("stop\n")
        self.enginegton.wait_for_ok()

    def process_engine_move(self, res):
        if len(res) < 2:
            return
        if (res[0] != "f" or res[1] != "/") or not self.enginegton.searching:
            return
        self.enginegton.searching = False
        if len(res) == 7:
            move = [int(res[i]) for i in range(2, len(res))]
        else:
            move = [int(res[i]) for i in range(2, 6)] + [int(res[6] + res[7])]
        self.enginegton.icon_rot = 0
        if self.current_game.engine_search_orientation != self.current_game.user_orientation:
            Position.reverse_move(move)
        if not self.current_game.engine_on:
            self.board.move_graphic = MoveGraphic((move[0], move[1]), (move[2], move[3]), self.window_data.sqr_size)
            return
        self.move_made(position=self.board.process_move(self.board.position[move[0]][move[1]], (move[0], move[1]),
                                                        (move[2], move[3]), **self.current_game.get_kwargs(move[4])))

    def game_event(self, **kwargs):
        event_type = kwargs["board"]
        if event_type > 1 and "winner" in kwargs:
            pygame.mixer.Sound.play(self.sounds[0])
            self.current_UI.get_game_over_UI(self.window_data, self.current_game.from_position, kwargs["winner"])
        self.board.board_situation = event_type

    def enable_draw_claim(self):
        self.current_UI.add_special_button(self.window_data, "claim")

    def restart_game(self, **kwargs):
        delattr(self.current_UI, "winner")
        self.current_UI = GeneralUI(GeneralUI.get_game_UI, win_data=self.window_data, graphics=self.load_all_pieces(),
                                    engine_on=kwargs["on"])
        self.board.reset_board_UI(True)
        self.board.set_pos(self.current_game.start_data["position"])
        self.current_game.start_data["preexisting_tree"] \
            = self.current_game.all_variations if kwargs["tree_exists"] else None
        self.current_game.start_data["engine_on"] = kwargs["on"]
        self.current_game = Game(**self.current_game.start_data)
        self.handle_events(self.current_game.get_moves(self.enginegton))
        if self.current_game.from_position and not hasattr(self.current_UI, "winner"):
            self.current_UI.dropdowns[0].buttons.append(self.current_UI.add_special_button(self.window_data, "editor"))
        self.update_variations(self.current_game.current_position, type=0)
        if self.current_game.engine_on and self.current_game.turn == self.current_game.engine_turn:
            self.current_game.start_engine(self.enginegton)

    def get_new_piece(self, **kwargs):
        ID = kwargs["id"]
        if self.scene == 2:
            if (ID == 10 and self.king_count()[0] != 0) or (ID == -10 and self.king_count()[1] != 0):
                return
            self.current_UI.sub_interface.res_except(ID)
        self.board.selected_piece = ID

    def clear_editor_selection(self):
        self.board.selected_piece = None

    # this function verifies if the user's castling choices in the editor are valid
    def verify_castling_choices(self, castle_choices):
        castle = castle_choices
        if self.board.position[7][4] != 10 or self.board.position[7][7] != 5:
            castle -= 8
        elif self.board.position[7][4] != 10 or self.board.position[7][0] != 5:
            castle -= 4
        elif self.board.position[0][4] != -10 or self.board.position[0][7] != -5:
            castle -= 2
        elif self.board.position[0][4] != -10 or self.board.position[0][0] != -5:
            castle -= 1
        return castle

    # this function is used in confirming that a game can start from the editor
    def king_count(self):
        return sum(row.count(10) for row in self.board.position), sum(row.count(-10) for row in self.board.position)

    def reset_editor(self):
        self.board.clear_pos()
        self.board.reset_board_UI()
        self.current_UI.buttons = list(filter(lambda btn: btn.event_ID != 40, self.current_UI.buttons))
        self.current_UI.sub_interface.res_all()

    def reset_editor_piece_buttons(self, **kwargs):
        if kwargs["edited"]:
            self.current_UI.sub_interface.res_pieces()

    def flip_board(self):
        self.board.reset_board_UI()
        self.current_game.user_orientation = -self.current_game.user_orientation
        if self.current_game.current_position.orientation != self.current_game.user_orientation:
            self.current_game.current_position.reverse_rank_and_file()
            Board.flip(self.current_game.current_position.position)
        Board.flip(self.board.position)
        self.board.flip_file_notation(self.window_data.sqr_size)
        for move in self.current_game.legal_moves:
            Position.reverse_move(move)

    def take_to_position(self, **kwargs):  # on notation button press, this function loads the corresponding position
        self.current_game.set_to_position(kwargs["position_key"])
        if "type" in kwargs:
            self.update_variations(self.current_game.current_position, type=kwargs["type"])
        else:
            self.update_variations(self.current_game.current_position, type=-2)
        self.board.set_board(self.current_game.current_position)
        self.board.reset_board_UI()
        if self.enginegton.searching:
            self.stop_engine()
        self.handle_events(self.current_game.get_moves(self.enginegton))

    def take_back(self, **kwargs):
        self.current_game.switch_position(kwargs["direction"])
        if type(self.current_UI.sub_interface) != VariationsInterface:
            self.current_UI.get_variations_UI(win_data=self.window_data, engine_on=self.current_game.engine_on)
        self.update_variations(self.current_game.current_position, type=kwargs["direction"])
        self.board.set_board(self.current_game.current_position)
        self.board.reset_board_UI()
        if self.enginegton.searching:
            self.stop_engine()
        self.handle_events(self.current_game.get_moves(self.enginegton))

    def btn_take_back(self):  # takeback on Options->Takeback button press, works differently in vs. engine game
        if self.current_game.current_position == self.current_game.all_variations.root:
            return
        if not self.current_game.engine_on:
            self.take_back(direction=-1)
            return
        if self.enginegton.searching:
            self.stop_engine()
            self.delete_from_here()
        else:
            if self.current_game.current_position != Position.get_next(self.current_game.all_variations.root):
                self.take_back(direction=-1)
            self.delete_from_here()
            if self.current_game.turn == self.current_game.engine_turn:
                self.current_game.start_engine(self.enginegton)

    def delete_from_here(self):  # on user delete from here button press
        self.current_game.trim_tree()
        self.update_variations(self.current_game.current_position, type=2)
        self.board.set_board(self.current_game.current_position)
        self.board.reset_board_UI()
        if self.enginegton.searching:
            self.stop_engine()
        self.handle_events(self.current_game.get_moves(self.enginegton))

    def switch_branch(self, **kwargs):  # on user branch button press
        self.take_to_position(type=0, **kwargs)

    def minimise_branch(self, **kwargs):
        self.take_to_position(**kwargs)
        self.take_back(direction=-1)

    def promote_variation(self):  # on user promote variation button press
        branch_ancestor = self.current_game.current_position.find_branch_ancestor()
        ancestor_ind = branch_ancestor.get_sibling_index()
        if ancestor_ind == 0:
            return
        temp = branch_ancestor.parent.children[ancestor_ind - 1]
        branch_ancestor.parent.children[ancestor_ind - 1] = branch_ancestor
        branch_ancestor.parent.children[ancestor_ind] = temp
        self.update_variations(self.current_game.current_position, branches_redraw=branch_ancestor, type=0)

    # Graphics loaders - load assets from the assets folder and transform, returning them in dictionary form
    def load_kings(self):  # some places, like main menu, only need king graphics
        black_king_img = pygame.image.load(os.path.join("assets", "pieces", "black_king.png"))
        white_king_img = pygame.image.load(os.path.join("assets", "pieces", "white_king.png"))
        return {10: pygame.transform.scale(white_king_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                -10: pygame.transform.scale(black_king_img, (self.window_data.sqr_size, self.window_data.sqr_size))}

    def load_pieces(self):

        black_bishop_img = pygame.image.load(os.path.join("assets", "pieces", "black_bishop.png"))
        white_bishop_img = pygame.image.load(os.path.join("assets", "pieces", "white_bishop.png"))
        black_knight_img = pygame.image.load(os.path.join("assets", "pieces", "black_knight.png"))
        white_knight_img = pygame.image.load(os.path.join("assets", "pieces", "white_knight.png"))
        black_rook_img = pygame.image.load(os.path.join("assets", "pieces", "black_rook.png"))
        white_rook_img = pygame.image.load(os.path.join("assets", "pieces", "white_rook.png"))
        white_queen_img = pygame.image.load(os.path.join("assets", "pieces", "white_queen.png"))
        black_queen_img = pygame.image.load(os.path.join("assets", "pieces", "black_queen.png"))

        return {2: pygame.transform.scale(white_knight_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                3: pygame.transform.scale(white_bishop_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                5: pygame.transform.scale(white_rook_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                9: pygame.transform.scale(white_queen_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                -2: pygame.transform.scale(black_knight_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                -3: pygame.transform.scale(black_bishop_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                -5: pygame.transform.scale(black_rook_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                -9: pygame.transform.scale(black_queen_img, (self.window_data.sqr_size, self.window_data.sqr_size))}

    def load_pawns(self):
        black_pawn_img = pygame.image.load(os.path.join("assets", "pieces", "black_pawn.png"))
        white_pawn_img = pygame.image.load(os.path.join("assets", "pieces", "white_pawn.png"))
        return {-1: pygame.transform.scale(black_pawn_img, (self.window_data.sqr_size, self.window_data.sqr_size)),
                1: pygame.transform.scale(white_pawn_img, (self.window_data.sqr_size, self.window_data.sqr_size))}

    def load_all_pieces(self):
        pieces = self.load_kings()
        pieces.update(self.load_pieces())
        pieces.update(self.load_pawns())
        return pieces

    def load_menu(self):
        icons = {0: pygame.transform.scale(pygame.image.load(os.path.join("assets", "img", "icon.png")),
                                           (self.window_data.sqr_size, self.window_data.sqr_size))}
        icons.update(self.load_kings())
        return icons

    def load_board_graphic(self):
        board = pygame.image.load(os.path.join("assets", "img", "board.png"))
        return pygame.transform.scale(board, (self.window_data.sqr_size * 8, self.window_data.sqr_size * 8))

    # this function is called on pygame video resize; it defines new app dimensions, and uses them to remake
    # graphics and the current UI
    def resize_application(self, height):
        self.window_data = WindowData(self.get_dimensions(height))
        if self.scene == -1:
            self.enginegton.icon = pygame.transform.scale(pygame.image.load(os.path.join("assets", "img", "icon.png")),
                                                          (self.window_data.sqr_size * 2,
                                                           self.window_data.sqr_size * 2))
            return
        self.enginegton.icon = pygame.transform.scale(pygame.image.load(os.path.join("assets", "img", "icon.png")),
                                                      (self.window_data.sqr_size // 2, self.window_data.sqr_size // 2))
        self.board.redraw(self.load_all_pieces(), self.load_board_graphic(), self.window_data.sqr_size)
        engine = None if self.current_game is None else self.current_game.engine_on
        self.current_UI.redraw(win_data=self.window_data, graphics=self.load_all_pieces(),
                               engine_on=engine)
        if self.scene == 1 and type(self.current_UI.sub_interface) == VariationsInterface:
            # noinspection PyUnresolvedReferences
            self.update_variations(self.current_game.current_position, type=0)

    @staticmethod
    def get_default_height():
        return pygame.display.get_desktop_sizes()[0][1] // 2

    @staticmethod
    def get_dimensions(height):  # all dimensions are dependent on the height obtained in the function before
        side_margin = height // 2
        screen_width = height + side_margin
        square_size = (screen_width - side_margin) // 8
        return screen_width, height, square_size, side_margin
