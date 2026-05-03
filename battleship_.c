#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define BOARD_SIZE 10
#define SHIP_COUNT 5
#define NAME_SIZE 32

#define CELL_WATER '~'
#define CELL_SHIP 'S'
#define CELL_HIT 'X'
#define CELL_MISS 'o'

typedef struct
{
    const char *name;
    int length;
} ShipSpec;

typedef struct
{
    char name[NAME_SIZE];
    char board[BOARD_SIZE][BOARD_SIZE];
    char tracking[BOARD_SIZE][BOARD_SIZE];
    int ships_remaining;
    int is_computer;
} Player;

static const ShipSpec g_ships[SHIP_COUNT] = {
    { "Carrier", 5 },
    { "Battleship", 4 },
    { "Cruiser", 3 },
    { "Submarine", 3 },
    { "Destroyer", 2 }
};

static void init_grid(char grid[BOARD_SIZE][BOARD_SIZE]);
static void init_player(Player *player);
static void clear_screen_lines(void);
static void wait_for_enter(void);
static void read_line(char *buffer, size_t size);
static int prompt_game_mode(void);
static int prompt_setup_mode(const Player *player);
static void prompt_player_name(Player *player, int player_number);
static void print_column_headers(void);
static void print_single_board(char grid[BOARD_SIZE][BOARD_SIZE], int hide_ships);
static void print_turn_view(const Player *current);
static int parse_coordinate(const char *text, int *row, int *col);
static int prompt_coordinate(const char *prompt, int *row, int *col);
static int prompt_orientation(void);
static int can_place_ship(char board[BOARD_SIZE][BOARD_SIZE], int row, int col, int length, int horizontal);
static void place_ship(char board[BOARD_SIZE][BOARD_SIZE], int row, int col, int length, int horizontal);
static int random_int(int max_value);
static void setup_ships(Player *player);
static void setup_random_ships(Player *player);
static int count_ship_cells(char board[BOARD_SIZE][BOARD_SIZE]);
static int all_ships_sunk(const Player *player);
static void announce_shot_result(int result);
static int take_shot(Player *attacker, Player *defender);

static void init_grid(char grid[BOARD_SIZE][BOARD_SIZE])
{
    int row;
    int col;

    for (row = 0; row < BOARD_SIZE; ++row)
    {
        for (col = 0; col < BOARD_SIZE; ++col)
        {
            grid[row][col] = CELL_WATER;
        }
    }
}

static void init_player(Player *player)
{
    player->name[0] = '\0';
    init_grid(player->board);
    init_grid(player->tracking);
    player->ships_remaining = 0;
    player->is_computer = 0;
}

static void clear_screen_lines(void)
{
    int i;

    for (i = 0; i < 30; ++i)
    {
        putchar('\n');
    }
}

static void wait_for_enter(void)
{
    char buffer[8];

    printf("Press Enter to continue...");
    read_line(buffer, sizeof(buffer));
}

static void read_line(char *buffer, size_t size)
{
    size_t length;

    if (fgets(buffer, (int) size, stdin) == NULL)
    {
        buffer[0] = '\0';
        return;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n')
    {
        buffer[length - 1] = '\0';
    }
    else
    {
        int ch;
        do
        {
            ch = getchar();
        } while (ch != '\n' && ch != EOF);
    }
}

static int prompt_game_mode(void)
{
    char buffer[16];
    int mode;

    for (;;)
    {
        printf("Select game mode:\n");
        printf("1. Two players\n");
        printf("2. Play against the computer\n");
        printf("Choice: ");
        read_line(buffer, sizeof(buffer));

        if (sscanf(buffer, "%d", &mode) == 1 && (mode == 1 || mode == 2))
        {
            return mode;
        }

        printf("Please enter 1 or 2.\n\n");
    }
}

static int prompt_setup_mode(const Player *player)
{
    char buffer[16];
    int mode;

    for (;;)
    {
        printf("\n%s, choose ship placement mode:\n", player->name);
        printf("1. Place ships manually\n");
        printf("2. Place ships randomly\n");
        printf("Choice: ");
        read_line(buffer, sizeof(buffer));

        if (sscanf(buffer, "%d", &mode) == 1 && (mode == 1 || mode == 2))
        {
            return mode;
        }

        printf("Please enter 1 or 2.\n");
    }
}

static void prompt_player_name(Player *player, int player_number)
{
    char buffer[NAME_SIZE];

    for (;;)
    {
        printf("Enter name for Player %d: ", player_number);
        read_line(buffer, sizeof(buffer));

        if (buffer[0] != '\0')
        {
            strcpy(player->name, buffer);
            break;
        }

        printf("Name cannot be empty.\n");
    }
}

static void print_column_headers(void)
{
    int col;

    printf("   ");
    for (col = 0; col < BOARD_SIZE; ++col)
    {
        printf(" %d", col + 1);
        if (col + 1 < 10)
        {
            printf(" ");
        }
    }
    printf("\n");
}

static void print_single_board(char grid[BOARD_SIZE][BOARD_SIZE], int hide_ships)
{
    int row;
    int col;
    char cell;

    print_column_headers();
    for (row = 0; row < BOARD_SIZE; ++row)
    {
        printf(" %c ", (char) ('A' + row));
        for (col = 0; col < BOARD_SIZE; ++col)
        {
            cell = grid[row][col];
            if (hide_ships && cell == CELL_SHIP)
            {
                cell = CELL_WATER;
            }
            printf(" %c ", cell);
        }
        printf("\n");
    }
}

static void print_turn_view(const Player *current)
{
    if (current->is_computer)
    {
        printf("\nComputer targeting board:\n");
        print_single_board((char (*)[BOARD_SIZE]) current->tracking, 0);
        return;
    }

    printf("\n%s's fleet:\n", current->name);
    print_single_board((char (*)[BOARD_SIZE]) current->board, 0);
    printf("\n%s's targeting board:\n", current->name);
    print_single_board((char (*)[BOARD_SIZE]) current->tracking, 0);
}

static int parse_coordinate(const char *text, int *row, int *col)
{
    int local_col;
    char letter;
    char extra;

    if (sscanf(text, " %c%d %c", &letter, &local_col, &extra) != 2)
    {
        return 0;
    }

    letter = (char) toupper((unsigned char) letter);
    if (letter < 'A' || letter >= 'A' + BOARD_SIZE)
    {
        return 0;
    }

    if (local_col < 1 || local_col > BOARD_SIZE)
    {
        return 0;
    }

    *row = letter - 'A';
    *col = local_col - 1;
    return 1;
}

static int prompt_coordinate(const char *prompt, int *row, int *col)
{
    char buffer[32];

    for (;;)
    {
        printf("%s", prompt);
        read_line(buffer, sizeof(buffer));
        if (parse_coordinate(buffer, row, col))
        {
            return 1;
        }
        printf("Invalid coordinate. Use values like A1, C7, or J10.\n");
    }
}

static int prompt_orientation(void)
{
    char buffer[16];
    char value;

    for (;;)
    {
        printf("Orientation ([H]orizontal/[V]ertical): ");
        read_line(buffer, sizeof(buffer));

        if (sscanf(buffer, " %c", &value) == 1)
        {
            value = (char) toupper((unsigned char) value);
            if (value == 'H')
            {
                return 1;
            }
            if (value == 'V')
            {
                return 0;
            }
        }

        printf("Please enter H or V.\n");
    }
}

static int can_place_ship(char board[BOARD_SIZE][BOARD_SIZE], int row, int col, int length, int horizontal)
{
    int i;
    int target_row;
    int target_col;

    for (i = 0; i < length; ++i)
    {
        target_row = row + (horizontal ? 0 : i);
        target_col = col + (horizontal ? i : 0);

        if (target_row < 0 || target_row >= BOARD_SIZE || target_col < 0 || target_col >= BOARD_SIZE)
        {
            return 0;
        }

        if (board[target_row][target_col] != CELL_WATER)
        {
            return 0;
        }
    }

    return 1;
}

static void place_ship(char board[BOARD_SIZE][BOARD_SIZE], int row, int col, int length, int horizontal)
{
    int i;

    for (i = 0; i < length; ++i)
    {
        board[row + (horizontal ? 0 : i)][col + (horizontal ? i : 0)] = CELL_SHIP;
    }
}

static int random_int(int max_value)
{
    if (max_value <= 0)
    {
        return 0;
    }

    return rand() % max_value;
}

static void setup_ships(Player *player)
{
    int i;
    int row;
    int col;
    int horizontal;
    int setup_mode;

    clear_screen_lines();
    setup_mode = prompt_setup_mode(player);

    if (setup_mode == 2)
    {
        setup_random_ships(player);
        printf("\n%s's ships were placed randomly.\n", player->name);
        print_single_board(player->board, 0);
        wait_for_enter();
        return;
    }

    printf("\n%s, place your ships.\n", player->name);

    for (i = 0; i < SHIP_COUNT; ++i)
    {
        for (;;)
        {
            printf("\nCurrent board:\n");
            print_single_board(player->board, 0);
            printf("\nPlace %s (length %d).\n", g_ships[i].name, g_ships[i].length);
            prompt_coordinate("Starting coordinate: ", &row, &col);
            horizontal = prompt_orientation();

            if (can_place_ship(player->board, row, col, g_ships[i].length, horizontal))
            {
                place_ship(player->board, row, col, g_ships[i].length, horizontal);
                break;
            }

            printf("That ship does not fit there or overlaps another ship.\n");
        }
    }

    player->ships_remaining = count_ship_cells(player->board);
    printf("\n%s is done placing ships.\n", player->name);
    wait_for_enter();
}

static void setup_random_ships(Player *player)
{
    int i;
    int row;
    int col;
    int horizontal;

    for (i = 0; i < SHIP_COUNT; ++i)
    {
        do
        {
            row = random_int(BOARD_SIZE);
            col = random_int(BOARD_SIZE);
            horizontal = random_int(2);
        } while (!can_place_ship(player->board, row, col, g_ships[i].length, horizontal));

        place_ship(player->board, row, col, g_ships[i].length, horizontal);
    }

    player->ships_remaining = count_ship_cells(player->board);
}

static int count_ship_cells(char board[BOARD_SIZE][BOARD_SIZE])
{
    int row;
    int col;
    int total;

    total = 0;
    for (row = 0; row < BOARD_SIZE; ++row)
    {
        for (col = 0; col < BOARD_SIZE; ++col)
        {
            if (board[row][col] == CELL_SHIP)
            {
                ++total;
            }
        }
    }
    return total;
}

static int all_ships_sunk(const Player *player)
{
    return player->ships_remaining <= 0;
}

static void announce_shot_result(int result)
{
    if (result)
    {
        printf("Hit!\n");
    }
    else
    {
        printf("Miss.\n");
    }
}

static int take_shot(Player *attacker, Player *defender)
{
    int row;
    int col;
    char *target_cell;

    for (;;)
    {
        clear_screen_lines();
        printf("%s's turn.\n", attacker->name);
        print_turn_view(attacker);
        printf("\nFire at %s's board.\n", defender->name);

        if (attacker->is_computer)
        {
            do
            {
                row = random_int(BOARD_SIZE);
                col = random_int(BOARD_SIZE);
            } while (attacker->tracking[row][col] == CELL_HIT || attacker->tracking[row][col] == CELL_MISS);

            printf("Computer fires at %c%d.\n", (char) ('A' + row), col + 1);
        }
        else
        {
            prompt_coordinate("Target coordinate: ", &row, &col);
        }

        if (attacker->tracking[row][col] == CELL_HIT || attacker->tracking[row][col] == CELL_MISS)
        {
            printf("You already fired at that coordinate.\n");
            wait_for_enter();
            continue;
        }

        target_cell = &defender->board[row][col];
        if (*target_cell == CELL_SHIP)
        {
            *target_cell = CELL_HIT;
            attacker->tracking[row][col] = CELL_HIT;
            defender->ships_remaining--;
            announce_shot_result(1);
            return 1;
        }

        attacker->tracking[row][col] = CELL_MISS;
        if (*target_cell == CELL_WATER)
        {
            *target_cell = CELL_MISS;
        }
        announce_shot_result(0);
        return 0;
    }
}

int main(void)
{
    Player player1;
    Player player2;
    Player *current;
    Player *opponent;
    int game_mode;

    init_player(&player1);
    init_player(&player2);
    srand((unsigned int) time(NULL));

    printf("=== Battleship ===\n\n");
    game_mode = prompt_game_mode();
    prompt_player_name(&player1, 1);
    setup_ships(&player1);

    if (game_mode == 1)
    {
        prompt_player_name(&player2, 2);
        setup_ships(&player2);
    }
    else
    {
        strcpy(player2.name, "Computer");
        player2.is_computer = 1;
        setup_random_ships(&player2);
        printf("\nThe computer has placed its ships.\n");
        wait_for_enter();
    }

    current = &player1;
    opponent = &player2;

    for (;;)
    {
        take_shot(current, opponent);

        if (all_ships_sunk(opponent))
        {
            printf("\n%s wins! All enemy ships have been sunk.\n", current->name);
            break;
        }

        if (!current->is_computer && !opponent->is_computer)
        {
            wait_for_enter();
        }
        else if (current->is_computer)
        {
            wait_for_enter();
        }

        if (current == &player1)
        {
            current = &player2;
            opponent = &player1;
        }
        else
        {
            current = &player1;
            opponent = &player2;
        }
    }

    return 0;
}
