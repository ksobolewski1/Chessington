from Buttons import *
import os


class GeneralUI:
    def __init__(self, layout, **kwargs):
        self.buttons = []
        self.dropdowns = []
        self.background = None
        self.sub_interface = None  # sub-interfaces include: EditorInterface in Board Editor, VariationsUI in Analysis,
        # PromotionUI in Analysis or vs. engine
        self.layout = layout  # the layout is stored in this member for the redraw() function
        layout(self, **kwargs)  # "layout" is one of the "get_" methods of GeneralUI (get menu, get editor, get game)

    def update(self, window_data, user_input, **kwargs):

        events = []
        if self.background is not None:
            window_data.window.blit(self.background, (0, 0))
        else:
            window_data.window.fill((180, 180, 180), (0, 0, pygame.display.get_surface().get_size()[0],
                                                      pygame.display.get_surface().get_size()[1]))

        for btn in self.buttons:
            if type(btn) != ToggleButton:
                btn.update(window_data, user_input, events)
            else:
                btn.update_toggle(window_data, user_input, events)
        for btn in self.dropdowns:
            btn.update_dropdown(window_data, user_input, events)
        if self.dropdowns_off() and self.sub_interface is not None:
            self.sub_interface.update(window_data, user_input, events, **kwargs)

        if hasattr(self, "winner"):
            Button.draw_text(window_data, getattr(self, "winner"),
                             pygame.font.SysFont("Futura", window_data.sqr_size // 2), (0, 0, 0),
                             window_data.width - window_data.side_margin_size + window_data.sqr_size // 2,
                             window_data.height // 2)

        return events

    def get_menu_UI(self, **kwargs):
        win_data = kwargs["win_data"]
        self.background = pygame.transform.scale(
            pygame.image.load(os.path.join("assets", "img", "bg.png")), (win_data.width, win_data.height))
        offsetX = win_data.width // 4
        offsetY = win_data.sqr_size // 2
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)
        white_btn = Button(0, 0, kwargs["graphics"][10], font, (222, 227, 230), 24,
                           pl_colour=1, side_to_move=1, from_pos=False, engine_on=True, preexisting_tree=None)
        black_btn = Button(0, 0, kwargs["graphics"][-10], font, (222, 227, 230), 24,
                           pl_colour=-1, side_to_move=1, from_pos=False, engine_on=True, preexisting_tree=None)
        play_btn = Dropdown(offsetX // 4, offsetY, "Play", font, (222, 227, 230), -1, [white_btn, black_btn])
        analysis_btn = Button(play_btn.rect.x + offsetX, offsetY, "Analysis", font, (222, 227, 230), 24,
                              pl_colour=1, side_to_move=1, from_pos=False, engine_on=False, preexisting_tree=None)
        editor_btn = Button(analysis_btn.rect.x + offsetX, offsetY, "Board editor", font, (222, 227, 230), 23)
        quit_btn = Button(editor_btn.rect.x + offsetX, offsetY, "Quit", font, (222, 227, 230), 21)
        self.buttons = [analysis_btn, editor_btn, quit_btn]
        self.dropdowns = [play_btn]

    def get_game_UI(self, **kwargs):

        win_data = kwargs["win_data"]
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)

        drop = [Button(0, 0, "Main menu", font, (0, 0, 0), 22), Button(0, 0, "Flip board", font, (0, 0, 0), 41),
                Button(0, 0, "Takeback", font, (0, 0, 0), 42)]
        if not kwargs["engine_on"]:
            drop.append(Button(0, 0, "Engine move", font, (0, 0, 0), 4))
        options_btn = Dropdown(win_data.width - win_data.side_margin_size + win_data.sqr_size, win_data.sqr_size // 4,
                               "Options", font, (0, 0, 0), 0, drop)

        self.dropdowns = [options_btn]
        if "promotion" in kwargs:
            if kwargs["promotion"]:
                self.get_promotion_UI(**kwargs, position=self.sub_interface.position_data)
                return
        self.get_variations_UI(**kwargs)

    def get_editor_UI(self, **kwargs):

        win_data = kwargs["win_data"]
        graphics = kwargs["graphics"]
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)

        self.sub_interface = EditorInterface()
        white_pawn_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size, win_data.sqr_size,
                                      graphics[1], font, (222, 227, 230), 5, id=1)
        black_pawn_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                      win_data.sqr_size, graphics[-1], font, (222, 227, 230), 5, id=-1)
        white_knight_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                                        win_data.sqr_size * 2, graphics[2], font, (222, 227, 230), 5, id=2)
        black_knight_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                        win_data.sqr_size * 2, graphics[-2], font, (222, 227, 230), 5, id=-2)
        white_bishop_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                                        win_data.sqr_size * 3, graphics[3], font, (222, 227, 230), 5, id=3)
        black_bishop_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                        win_data.sqr_size * 3, graphics[-3], font, (222, 227, 230), 5, id=-3)
        white_rook_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                                      win_data.sqr_size * 4, graphics[5], font, (222, 227, 230), 5, id=5)
        black_rook_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                      win_data.sqr_size * 4, graphics[-5], font, (222, 227, 230), 5, id=-5)
        white_queen_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                                       win_data.sqr_size * 5, graphics[9], font, (222, 227, 230), 5, id=9)
        black_queen_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                       win_data.sqr_size * 5, graphics[-9], font, (222, 227, 230), 5, id=-9)
        white_king_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                                      win_data.sqr_size * 6, graphics[10], font, (222, 227, 230), 5, id=10)
        black_king_btn = ToggleButton(win_data.width - win_data.side_margin_size + win_data.sqr_size * 2,
                                      win_data.sqr_size * 6, graphics[-10], font, (222, 227, 230), 5, id=-10)

        white_short_btn = ToggleButton(white_pawn_btn.rect.x - win_data.sqr_size * 0.8, white_king_btn.rect.y, "0-0",
                                       font, (0, 0, 0), -11)
        white_long_btn = ToggleButton(white_short_btn.rect.x, white_short_btn.rect.y + win_data.sqr_size // 2, "0-0-0",
                                      font, (0, 0, 0), -12)
        black_short_btn = ToggleButton(black_pawn_btn.rect.x + win_data.sqr_size * 1.2, white_king_btn.rect.y, "0-0",
                                       font, (0, 0, 0), -13)
        black_long_btn = ToggleButton(black_short_btn.rect.x, black_short_btn.rect.y + win_data.sqr_size // 2, "0-0-0",
                                      font, (0, 0, 0), -14)

        self.sub_interface.piece_buttons = [white_pawn_btn, black_pawn_btn, white_knight_btn, black_knight_btn,
                                            white_bishop_btn, black_bishop_btn, white_rook_btn, black_rook_btn,
                                            white_queen_btn, black_queen_btn, white_king_btn, black_king_btn]
        self.sub_interface.castling_buttons = [white_short_btn, white_long_btn, black_short_btn, black_long_btn]

        menu_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size,
                          white_king_btn.rect.y + win_data.sqr_size + win_data.sqr_size // 4, "Main menu",
                          font, (0, 0, 0), 22)

        white_analysis_btn = Button(0, 0, "White to move", font, (0, 0, 0), 24,
                                    pl_colour=1, side_to_move=1, from_pos=True, engine_on=False, preexisting_tree=None)
        black_analysis_btn = Button(0, 0, "Black to move", font, (0, 0, 0), 24,
                                    pl_colour=1, side_to_move=-1, from_pos=True, engine_on=False, preexisting_tree=None)
        analysis_btn = Dropdown(win_data.width - win_data.side_margin_size + win_data.sqr_size // 3,
                                white_pawn_btn.rect.y - win_data.sqr_size // 2, "Analysis", font, (0, 0, 0), 3,
                                [white_analysis_btn, black_analysis_btn])

        white_btn = Button(0, 0, graphics[10], font, (222, 227, 230), 24,
                           pl_colour=1, side_to_move=1, from_pos=True, engine_on=True, preexisting_tree=None)
        black_btn = Button(0, 0, graphics[-10], font, (222, 227, 230), 24,
                           pl_colour=-1, side_to_move=1, from_pos=True, engine_on=True, preexisting_tree=None)
        play_btn = Dropdown(analysis_btn.rect.x + win_data.sqr_size * 2.5, analysis_btn.rect.y,
                            "Play", font, (0, 0, 0), 0, [white_btn, black_btn])

        self.buttons = [menu_btn]
        self.dropdowns = [play_btn, analysis_btn]

    # special buttons appear on condition, and are not part of the static UI
    def add_special_button(self, win_data, tag):

        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)
        if tag == "reset":
            return Button(self.buttons[0].rect.x,
                          self.buttons[0].rect.y + self.buttons[0].rect.h, "Reset", font, (0, 0, 0), 40)
        elif tag == "editor":
            return Button(0, 0, "Return to editor", font, (0, 0, 0), 23)
        elif tag == "claim":
            self.dropdowns[0].buttons.append(
                Button(0, 0, "Claim draw", font, (0, 0, 0), 50, board=3, winner="Threefold repetition"))

    def remove_special_button(self, tag):
        if tag == "claim" and self.dropdowns[0].buttons[-1].event_ID == 50:
            self.dropdowns[0].buttons.pop()

    def get_variations_UI(self, **kwargs):
        win_data = kwargs["win_data"]
        self.sub_interface = VariationsInterface(win_data.width - win_data.side_margin_size + win_data.sqr_size // 4,
                                                 win_data.sqr_size, win_data.sqr_size * 2, win_data.sqr_size * 2,
                                                 kwargs["engine_on"])

    def get_game_over_UI(self, win_data, from_pos, winner):
        self.buttons.clear()
        self.dropdowns.clear()
        self.sub_interface = None
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)
        setattr(self, "winner", winner)
        menu_btn = Button(0, 0, "Main menu", font, (0, 0, 0), 22)
        rematch_btn = Button(0, 0, "Rematch", font, (0, 0, 0), 33, tree_exists=False, on=True)
        analysis_btn = Button(0, 0, "Analyse", font, (0, 0, 0), 33, tree_exists=True, on=False)

        if from_pos:
            drop = [menu_btn, self.add_special_button(win_data, "editor"), rematch_btn, analysis_btn]
        else:
            drop = [menu_btn, rematch_btn, analysis_btn]
        options_btn = Dropdown(win_data.width - win_data.side_margin_size + win_data.sqr_size, win_data.sqr_size // 4,
                               "Options", font, (0, 0, 0), 0, drop)
        self.dropdowns = [options_btn]

    def get_promotion_UI(self, **kwargs):
        self.sub_interface = PromotionInterface(**kwargs)

    def redraw(self, **kwargs):  # called when application resizes
        self.buttons.clear()
        self.dropdowns.clear()
        self.background = None
        self.layout(self, **kwargs, promotion=type(self.sub_interface) == PromotionInterface)

    def dropdowns_off(self):
        for btn in self.dropdowns:
            if btn.dropdown_rect is not None:
                return False
        return True


class EditorInterface:
    def __init__(self):
        self.piece_buttons = []
        self.castling_buttons = []

    def update(self, window_data, user_input, events, **kwargs):
        buttons_on = 0
        for btn in self.castling_buttons:
            btn.update_toggle(window_data, user_input, events)
        for btn in self.piece_buttons:
            btn.update_toggle(window_data, user_input, events)
            if btn.on == 1:
                buttons_on = 1
        if buttons_on == 0:
            events.append(Event(6))

    def res_pieces(self):
        for btn in self.piece_buttons:
            btn.on = 0

    def res_all(self):
        self.res_pieces()
        for btn in self.castling_buttons:
            btn.on = 0

    def res_except(self, ID):  # resets piece toggle buttons except one
        for btn in self.piece_buttons:
            btn.on = 0 if btn.event_data["id"] != ID else 1

    def get_castling_choices(self):  # reads out which castling options are selected
        return sum(8 / (2 ** (3 - i)) for i, button in enumerate(self.castling_buttons) if button.on)


class PromotionInterface:
    def __init__(self, **kwargs):
        self.buttons = []
        self.position_data = kwargs["position"]
        self.side = self.position_data.turn
        self.create(self.side, kwargs["win_data"], kwargs["graphics"])

    def update(self, window_data, user_input, events, **kwargs):
        for btn in self.buttons:
            btn.update(window_data, user_input, events)
        return events

    def create(self, side, win_data, graphics):
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 2)
        half = win_data.sqr_size // 2
        if side == 1:
            white_knight_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                      win_data.sqr_size * 2 + half, graphics[2], font, (222, 227, 230), 44, id=2)
            white_bishop_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                      win_data.sqr_size * 3 + half, graphics[3], font, (222, 227, 230), 44, id=3)
            white_rook_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                    win_data.sqr_size * 4 + half, graphics[5], font, (222, 227, 230), 44, id=5)
            white_queen_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                     win_data.sqr_size * 5 + half,
                                     graphics[9], font, (222, 227, 230), 44, id=9)
            self.buttons = [white_knight_btn, white_bishop_btn, white_rook_btn, white_queen_btn]
        else:
            black_knight_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                      win_data.sqr_size * 2 + half, graphics[-2], font, (222, 227, 230), 44, id=-2)
            black_bishop_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                      win_data.sqr_size * 3 + half, graphics[-3], font, (222, 227, 230), 44, id=-3)
            black_rook_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                    win_data.sqr_size * 4 + half, graphics[-5], font, (222, 227, 230), 44, id=-5)
            black_queen_btn = Button(win_data.width - win_data.side_margin_size + win_data.sqr_size + half,
                                     win_data.sqr_size * 5 + half, graphics[-9], font, (222, 227, 230), 44, id=-9)
            self.buttons = [black_knight_btn, black_bishop_btn, black_rook_btn, black_queen_btn]


class VariationsInterface:
    def __init__(self, x, y, width, height, engine=False):

        self.pos_buttons = {0: []}
        max_height = height // 2
        if not engine:
            self.scrollers = {0: ScrollingInterface(x, y, width, max_height, 4),
                              1: ScrollingInterface(x, y + height, width, max_height, 4),
                              2: ScrollingInterface(x + width, y + height, width, max_height, 4),
                              3: ScrollingInterface(x, y + height * 2, width, max_height, 4),
                              4: ScrollingInterface(x + width, y + height * 2, width, max_height, 4)}
        else:
            self.scrollers = {0: ScrollingInterface(x, y, width, height * 2, 16)}

        self.branch_buttons = {0: []}  # notation buttons that point to an alternative line
        self.modify_buttons = []  # Delete from here, promote variation, make main line
        self.clock = time.time()

    def update(self, window_data, user_input, events, **kwargs):
        if kwargs["engine"]:
            self.engine_update(window_data, user_input)
        else:
            self.analysis_update(window_data, user_input, events)

    def analysis_update(self, window_data, user_input, events):

        # Keyboard press takeback
        if time.time() - self.clock > 0.15 and user_input.arrow_press != 0:
            events.append(Event(1, direction=user_input.arrow_press))
            self.clock = time.time()

        # Update notation buttons and scrollers
        for i in range(max(self.pos_buttons.keys()) + 1):
            scr = self.scrollers[i].update(user_input)
            for btn in self.pos_buttons[i]:
                btn.update_notation(window_data, user_input, scr, events)

        for i in range(max(self.branch_buttons.keys()) + 1):
            for btn in self.branch_buttons[i]:
                btn.update_toggle(window_data, user_input, events)

        for btn in self.modify_buttons:
            btn.update(window_data, user_input, events)

        return events

    def engine_update(self, window_data, user_input):  # Many Analysis features of VariationsUI are disabled here
        scr = self.scrollers[0].update(user_input)
        for btn in self.pos_buttons[0]:
            btn.update_notation(window_data, user_input, scr)

    def get_notation_buttons(self, win_data, line, depth, key):
        # iterate over the line of Positions and fill in the buttons list
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 4)
        interface = self.scrollers[depth]
        if depth not in self.pos_buttons:
            self.pos_buttons[depth] = []
        buttons = []
        y_offset = 0 + interface.total
        y_increment = win_data.sqr_size // 8
        for index, pos in enumerate(line):
            if pos.turn == 1:
                buttons.append(
                    NotationButton(interface.rect.x, interface.rect.y + y_offset, pos.move_notation, font, (0, 0, 0), 0,
                                   position_key=pos.key))
            else:
                if index != 0:
                    btn_y = buttons[-1].rect.y
                else:
                    btn_y = interface.rect.y + interface.total
                    y_offset += win_data.sqr_size // 8
                buttons.append(
                    NotationButton(interface.rect.x + win_data.sqr_size, btn_y, pos.move_notation, font, (0, 0, 0), 0,
                                   position_key=pos.key))
            if pos.key == key:
                buttons[-1].on = 1
            else:
                buttons[-1].on = 0
            y_offset += y_increment
        self.pos_buttons[depth] = buttons

    def allow_delete(self, win_data):
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 4)
        where = self.scrollers[0]
        self.modify_buttons = [Button(
            where.rect.x + where.rect.w, where.rect.y, "Delete from here", font, (0, 0, 0), 2)]

    def allow_branch_promotions(self, win_data):
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 4)
        self.allow_delete(win_data)
        self.modify_buttons.append(Button(self.modify_buttons[-1].rect.x,
                                          self.modify_buttons[-1].rect.y + self.modify_buttons[-1].rect.h,
                                          "Promote variation", font, (0, 0, 0), 3))

    def no_modify(self):
        self.modify_buttons.clear()

    def no_branch(self, depth):
        self.branch_buttons = {key: value for key, value in self.branch_buttons.items() if key < depth}
        if len(self.branch_buttons.keys()) == 0:
            self.branch_buttons[0] = []
        self.pos_buttons_cut(depth)

    def pos_buttons_cut(self, depth):
        self.pos_buttons = {key: value for key, value in self.pos_buttons.items() if key <= depth}
        if len(self.pos_buttons.keys()) == 0:
            self.pos_buttons[0] = []

    def get_branch_buttons(self, win_data, siblings, depth, key):
        font = pygame.font.SysFont("Futura", win_data.sqr_size // 4)
        self.branch_buttons[depth] = []
        y_off = 0
        for alternative in siblings:
            btn = ToggleButton(self.scrollers[depth + 1].rect.x,
                               (self.scrollers[depth + 1].rect.y - win_data.sqr_size * 0.7) + y_off,
                               "=> " + alternative.move_notation + " ...", font, (0, 0, 0), 8,
                               position_key=alternative.key)
            if alternative.key == key:
                btn.on = True
            self.branch_buttons[depth].append(btn)
            y_off += btn.rect.h

    def update_scrolling_height(self, depth):
        self.scrollers[depth].update_height(self.get_buttons_height(self.pos_buttons[depth]))

    @staticmethod
    def get_buttons_height(button_group):
        if len(button_group) <= 0:
            return 0
        elif len(button_group) == 1:
            return button_group[0].rect.h
        else:
            return button_group[-1].rect.y - button_group[0].rect.y + button_group[-1].rect.h * 1.1


class ScrollingInterface:  # ScrollingInterface manipulates the offset for the height of NotationButton.rect
    def __init__(self, x, y, width, height, max_cap):
        self.rect = pygame.Rect(x, y, width, 0)
        self.max_height = height
        self.total = 0
        self.scrolling_max = 0
        self.delta = height // max_cap
        self.min_delta = self.delta // 10
        self.pre_off = 0
        self.max_down = False

    def update(self, user_input):

        offset = 0
        if self.total + self.pre_off <= 0:
            offset = self.pre_off
            self.total += self.pre_off
        self.pre_off = 0

        if self.max_down:
            self.to_max()

        if not self.rect.collidepoint(user_input.position) or user_input.wheel is None or self.scrolling_max <= 0:
            return [self.rect, self.adjust(offset)]

        reverse = -self.total
        if user_input.wheel == -1 and self.total - self.min_delta >= -self.scrolling_max:
            offset -= self.delta
            self.total -= self.delta
        elif user_input.wheel == 1 and self.total + self.min_delta <= reverse and self.total + self.delta <= 0:
            offset += self.delta
            self.total += self.delta

        return [self.rect, self.adjust(offset)]

    def forward_offset(self, turn):
        if turn == -1 or self.rect.h < self.max_height or self.total - self.min_delta <= -self.scrolling_max:
            return
        self.pre_off -= self.delta

    def back_offset(self, turn):
        if turn == 1 or self.rect.h < self.max_height or self.total + self.min_delta >= -self.total:
            return
        self.pre_off += self.delta

    def reset(self):
        self.pre_off = -self.total

    def reset_to_max(self):
        self.pre_off = -self.total
        self.max_down = True

    def to_max(self):
        self.pre_off = -self.scrolling_max
        self.max_down = False

    def adjust(self, off):
        if abs(self.total) > self.scrolling_max > 0:
            self.total = -self.scrolling_max
            off += self.scrolling_max + self.total
        return off

    def update_height(self, val):
        self.rect.h = self.max_height if val >= self.max_height else val
        self.scrolling_max = int(val - self.max_height)
