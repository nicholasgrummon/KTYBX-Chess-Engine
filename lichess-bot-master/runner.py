import lib.engine_wrapper as eng
import homemade
import chess
import subprocess

subprocess.run(["g++", "engines/full-version.cpp", "-o", "engines/ktybx"])

newBoard = chess.Board()

ktybx = eng.get_homemade_engine("KTYBX")
move = homemade.KTYBX.search(ktybx,newBoard)

print(move)
# ktybx.search(newBoard)