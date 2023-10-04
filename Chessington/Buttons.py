import pygame
import time
from Event import *


class Button:
    def __init__(self, x, y, display, font, font_colour, ID, **event_data):
        self.event_ID = ID
        self.event_data = event_data
        self.to_display = display
        self.font = font
        self.font_colour = font_colour
        if type(self.to_display) == str:
            self.rect = pygame.Rect(x, y, self.font.size(self.to_display)[0], self.font.size(self.to_display)[1])
        else:
            self.rect = pygame.Rect(x, y, self.to_display.get_size()[0], self.to_display.get_size()[1])
        self.clock = time.time()  # for button cooldown

    @staticmethod
    def draw_text(window_data, text, font, text_col, x, y):
        img = font.render(text, True, text_col)
        window_data.window.blit(img, (x, y))

    def update(self, window_data, user_input, event_list):

        mouse_collides = self.rect.collidepoint(user_input.position)

        if user_input.left_click and mouse_collides and time.time() - self.clock >= 0.6 and not user_input.dragging:
            event_list.append(Event(self.event_ID, **self.event_data))
            self.clock = time.time()

        if mouse_collides and not user_input.dragging:
            pygame.draw.rect(window_data.window, (127, 23, 52), self.rect)

        if type(self.to_display) == str:
            self.draw_text(window_data, self.to_display, self.font, self.font_colour, self.rect.x, self.rect.y)
        else:
            window_data.window.blit(self.to_display, (self.rect.x, self.rect.y))

        # the list addition makes sure that in case the main button is the widest, then its length is considered when
        # deriving the rect widths


class Dropdown(Button):
    def __init__(self, x, y, display, font, font_colour, ID, buttons):
        super().__init__(x, y, display, font, font_colour, ID)

        self.buttons = buttons
        self.dropdown_rect = None

    def update_dropdown(self, window_data, user_input, event_list):

        mouse_collides = self.rect.collidepoint(user_input.position)

        if mouse_collides and not user_input.dragging:
            self.get_dropdown(window_data.sqr_size)

        if self.dropdown_rect is not None and self.dropdown_rect.collidepoint(user_input.position):
            for btn in self.buttons:
                btn.update(window_data, user_input, event_list)
            pygame.draw.rect(window_data.window, (127, 23, 52), self.rect)
        else:
            self.dropdown_rect = None

        if type(self.to_display) == str:
            self.draw_text(window_data, self.to_display, self.font, self.font_colour, self.rect.x, self.rect.y)
        else:
            window_data.window.blit(self.to_display, (self.rect.x, self.rect.y))

    def get_dropdown(self, sqr_size):
        offset = sqr_size // 10
        height = self.rect.h
        for btn in self.buttons:
            btn.rect.y = self.rect.y + height + offset
            height += btn.rect.h + offset
            btn.rect.x = self.rect.x
            btn.rect.w = max(self.buttons + [self], key=lambda button: button.rect.w).rect.w
        self.rect.w = max(self.buttons + [self], key=lambda button: button.rect.w).rect.w
        self.dropdown_rect = pygame.Rect(self.rect.x, self.rect.y, self.rect.w, height)


class ToggleButton(Button):
    def __init__(self, x, y, display, font, font_colour, ID, **event_data):
        super().__init__(x, y, display, font, font_colour, ID, **event_data)
        self.on = 0

    def update_toggle(self, window_data, user_input, event_list):

        mouse_collides = self.rect.collidepoint(user_input.position)

        if self.on == 1 or (mouse_collides and not user_input.dragging):
            pygame.draw.rect(window_data.window, (127, 23, 52), self.rect)

        if mouse_collides and user_input.left_click and time.time() - self.clock >= 0.2 \
                and not user_input.dragging and self.on == 0:
            event_list.append(Event(self.event_ID, **self.event_data))
            self.on = 1
            self.clock = time.time()

        if mouse_collides and user_input.any_click() and time.time() - self.clock >= 0.2 and not user_input.dragging \
                and self.on == 1:
            # add an opposite-effect event
            event_list.append(Event(-self.event_ID, **self.event_data))
            self.on = 0
            self.clock = time.time()

        if type(self.to_display) == str:
            self.draw_text(window_data, self.to_display, self.font, self.font_colour, self.rect.x, self.rect.y)
        else:
            window_data.window.blit(self.to_display, (self.rect.x, self.rect.y))


class NotationButton(ToggleButton):
    def __init__(self, x, y, display, font, font_colour, ID, **event_data):
        super().__init__(x, y, display, font, font_colour, ID, **event_data)

    def update_notation(self, window_data, user_input, scroller, event_list=None):

        self.rect.y += scroller[1]
        if self.rect.y + self.rect.h <= scroller[0].y \
                or self.rect.y + self.rect.h >= scroller[0].y + scroller[0].h * 1.1:
            return

        if event_list is None:
            self.draw_text(window_data, self.to_display, self.font, self.font_colour, self.rect.x, self.rect.y)
        else:
            self.update_toggle(window_data, user_input, event_list)
