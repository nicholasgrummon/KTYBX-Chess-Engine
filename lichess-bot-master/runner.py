import lib.engine_wrapper as eng
import homemade
import chess
import subprocess

subprocess.run(["g++", "engines/full-version.cpp", "-o", "engines/ktybx"])

mate_in_1_puzzles = [
    "4r1k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1",
    "r7/p4p2/bp3q2/2Bk2r1/8/P6Q/4NP1P/5K2 b - - 0 1",
    "5k2/5p2/5P1p/p5p1/2PR4/7P/2r2qP1/6RK w - - 0 1",
    "r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 0",
    "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 0",
    "r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 1 0"
]

solutions = [
    "e1e8",
    "f6a1",
    "d4d8",
    "Nf6+",
    "Qb8+",
    "Qd8+",
]

# for fen in mate_in_1_puzzles:
fen = mate_in_1_puzzles[4]
puzzleBoard = chess.Board(fen)

ktybx = eng.get_homemade_engine("KTYBX")
move = homemade.KTYBX.search(ktybx,puzzleBoard)

print(move)