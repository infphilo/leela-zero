#!/usr/bin/env python
import sys, random
from argparse import ArgumentParser, FileType

def draw_game(play):
    game = [[0 for _ in range(19)] for _ in range(19)]
    for p in play:
        stone, _, x, y, _ = p
        x, y = ord(x) - ord('a'), ord(y) - ord('a')
        assert x >= 0 and x < 19 and y >= 0 and y < 19
        game[y][x] = 1 if stone == "W" else 2
            
    game_out = ""
    for y in range(len(game)):
        row = ""
        for x in range(len(game[y])):
            stone = game[y][x]
            if stone == 0:   # Empty
                row += "."
            elif stone == 1: # White
                row += 'O'
            else:            # Black
                assert stone == 2
                row += 'X'
        game_out += ("%s\n" % row)
        
    return game_out

def transform(play):
    plays = [play]
    def transform_impl(play, m):
        play2 = []
        for p in play:
            stone, _, x, y, _ = p
            x, y = ord(x) - ord('a'), ord(y) - ord('a')
            assert x >= 0 and x < 19 and y >= 0 and y < 19
            if m[0][0] != 0:
                nx = x if m[0][0] == 1 else 18 - x
            else:
                nx = y if m[0][1] == 1 else 18 - y
            if m[1][0] != 0:
                ny = x if m[1][0] == 1 else 18 - x
            else:
                ny = y if m[0][1] == 1 else 18 - y
            x, y = nx, ny
            x, y = chr(x + ord('a')), chr(y + ord('a'))
            play2.append("%s[%s%s]" % (stone, x, y))
        return play2

    plays.append(transform_impl(play, [[ 1,  0], [ 0, -1]]))
    plays.append(transform_impl(play, [[-1,  0], [ 0,  1]]))
    plays.append(transform_impl(play, [[-1,  0], [ 0, -1]]))
    plays.append(transform_impl(play, [[ 0,  1], [ 1,  0]]))
    plays.append(transform_impl(play, [[ 0,  1], [-1,  0]]))
    plays.append(transform_impl(play, [[ 0, -1], [ 1,  0]]))
    plays.append(transform_impl(play, [[ 0, -1], [-1,  0]]))
    return plays

def get_bound(play):
    l, t, r, b = 19, 19, -1, -1
    for p in play:
        stone, _, x, y, _ = p
        x, y = ord(x) - ord('a'), ord(y) - ord('a')
        assert x >= 0 and x < 19 and y >= 0 and y < 19
        if x < l:
            l = x
        if x > r:
            r = x
        if y < t:
            t = y
        if y > b:
            b = y

    return l, t, r, b

def shift(play, xs, ys):
    plays = []
    for p in play:
        stone, _, x, y, _ = p
        x, y = ord(x) - ord('a'), ord(y) - ord('a')
        assert x >= 0 and x < 19 and y >= 0 and y < 19
        x += xs
        y += ys
        assert x >= 0 and x < 19 and y >= 0 and y < 19
        x, y = chr(x + ord('a')), chr(y + ord('a'))
        plays.append("%s[%s%s]" % (stone, x, y))
    return plays

def extract_games(game_file,
                  out_num_games,
                  num_stones,
                  verbose):
    num_games = 0
    games, unique_games = {}, set()
    for line in game_file:
        line = line.strip()
        if line.startswith('('):
            game_header = line
            play = ""
        else:
            play += line
            if line.endswith(')'):
                num_games += 1
                if num_games % 10000 == 0:
                    print >> sys.stderr, "%d games are processed." % num_games
                play = play[:-1]
                play = play.split(';')[1:]
                orig_play = play = play[:num_stones]
                play_str = ';'.join(play)
                if play_str in unique_games:
                    continue
                unique_games.add(play_str)

                l, t, r, b = get_bound(play)
                for sx in [-1, 0, 1]:
                    if sx + l < 0 or sx + r >= 19:
                        continue
                    for sy in [-1, 0, 1]:
                        if sy + t < 0 or sy + b >= 19:
                            continue

                        play_shift = shift(orig_play, sx, sy)
                        play_shift_str = ';'.join(play_shift)
                        plays = transform(play_shift)

                        """
                        for play in plays:
                            play_out = draw_game(play)
                            print play_out
                        sys.exit(1)
                        """

                        for play in plays:
                            play = ';'.join(play)
                            if play in games:
                                continue
                            games[play] = game_header

    print >> sys.stderr, "# of games: %d, # of unique games: %d, # of reported games: %d" % (num_games, len(unique_games), len(games))

    plays = games.keys()
    random.shuffle(plays)
    if num_games > 0:
        game_keys = plays[:out_num_games]

    for play in plays:
        assert play in games
        print games[play]
        print
        print ';%s)' % play


if __name__ == '__main__':
    parser = ArgumentParser(
        description='Extract Games')
    parser.add_argument('game_file',
                        nargs='?',
                        type=FileType('r'),
                        help='input Game file (use "-" for stdin)')
    parser.add_argument('-n', '--num-games',
                        dest='num_games',
                        type=int,
                        default=0,
                        help='number of games to be randomly chosen and reported')
    parser.add_argument('--num-stones',
                        dest='num_stones',
                        type=int,
                        default=sys.maxint,
                        help='number of games to be randomly chosen and reported')
    parser.add_argument('-v', '--verbose',
                        dest='verbose',
                        action='store_true',
                        help='also print some statistics to stderr')

    args = parser.parse_args()
    if not args.game_file:
        parser.print_help()
        exit(1)
    extract_games(args.game_file,
                  args.num_games,
                  args.num_stones,
                  args.verbose)
    
