#!/usr/bin/env python
import sys, random
from argparse import ArgumentParser, FileType

def extract_games(game_file,
                  num_games,
                  num_stones,
                  verbose):
    unique_games, trans_unique_games, games = {}, {}, []
    dk_games = []
    for line in game_file:
        line = line.strip()
        if line.startswith('('):
            content = [line]
            play = ""
        else:
            content.append(line)
            play += line
            if line.endswith(')'):
                content = '\n'.join(content)
                games.append(content)
                play = play[:-1]
                play = play.split(';')[1:]
                play = play[:num_stones]

                def transform(play, xt, yt):
                    play2 = []
                    for p in play:
                        stone, _, x, y, _ = p
                        x, y = ord(x) - ord('a'), ord(y) - ord('a')
                        assert x >= 0 and x < 19 and y >= 0 and y < 19
                        if xt == 1:
                            x = 18 - x
                        if yt == 1:
                            y = 18 - y
                        x, y = chr(x + ord('a')), chr(y + ord('a'))
                        play2.append("%s[%s%s]" % (stone, x, y))
                    return play2

                play2 = transform(play, 1, 0)
                play3 = transform(play, 0, 1)
                play4 = transform(play, 1, 1)

                dk_play = set(play)
                play = ''.join(play)
                play2 = ''.join(play2)
                play3 = ''.join(play3)
                play4 = ''.join(play4)

                if play not in unique_games:
                    unique_games[play] = [len(games) - 1]
                    dk_games.append(dk_play)
                else:
                    unique_games[play].append(len(games) - 1)

                plays = [play, play2, play3, play4]
                plays.sort()
                play = plays[0]
                if play not in trans_unique_games:
                    trans_unique_games[play] = [len(games) - 1]
                else:
                    trans_unique_games[play].append(len(games) - 1)

    print >> sys.stderr, "# of games: %d, # of unique games: %d, # of transformed unique games: %d" % (len(games), len(unique_games), len(trans_unique_games))

    game_keys = trans_unique_games.keys()
    random.shuffle(game_keys)
    if num_games > 0:
        game_keys = game_keys[:num_games]

    for game_key in game_keys:
        assert game_key in trans_unique_games
        game_num = trans_unique_games[game_key][0]
        print games[game_num]

    dk_unique = [0 for _ in range(len(dk_games))]
    for i in range(len(dk_games)):
        if dk_unique[i] > 0:
            continue
        dk_play = dk_games[i]
        for j in range(i+1, len(dk_games)):
            if dk_unique[j] > 0:
                continue
            dk_play2 = dk_games[j]
            if dk_play == dk_play2:
                dk_unique[j] += 1

    dk_games2 = []
    for i in range(len(dk_unique)):
        if dk_unique[i] > 0:
            continue
        dk_games2.append(dk_games[i])

    print >> sys.stderr, len(dk_games2), len(dk_games)

    dk_games =  dk_games2
    dk_subset = [0 for _ in range(len(dk_games))]
    for i in range(len(dk_games)):
        if dk_subset[i] > 0:
            continue
        dk_play = dk_games[i]
        for j in range(i+1, len(dk_games)):
            dk_play2 = dk_games[j]
            assert dk_play != dk_play2
            if dk_play > dk_play2:
                dk_subset[j] += 1
            elif dk_play < dk_play2:
                dk_subset[i] += 1
                break

    subset_count = 0
    for sub in dk_subset:
        if sub == 0:
            subset_count += 1
    print >> sys.stderr, "Subset count:", subset_count
                



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
    
