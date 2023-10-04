from Chessington import *
from multiprocessing import freeze_support

pygame.init()
pygame.mixer.init()


def main():
    chessington = Chessington()

    while chessington.running:
        chessington.clock.tick(60)

        wheel = None
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                chessington.quit_app()
            elif event.type == pygame.VIDEORESIZE:
                chessington.resize_application(pygame.display.get_surface().get_size()[1])
            elif event.type == pygame.MOUSEWHEEL:
                wheel = event.y

        chessington.update_app(wheel)
        pygame.display.update()


if __name__ == '__main__':
    freeze_support()
    main()
