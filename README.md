# KTYBX-Chess-Engine

KITTY BOX COMMAND-LINE CHESS ENGINE
Developed by Nicholas Grummon c 2021, 2025
Kitty Box (KTYBX) is a chess engine written in C++

Project Goals:
  The goal of Kitty Box is to create a strong chess engine that is built without the help
  of external libraries or data structures (i.e. opening books, endgame tables, etc), so
  as to facilitate learning and understanding of complex computational and chess subjects.
 
Features:
  Command line chess engine that uses long algebraic notation and ASCII
  graphics to play chess. A simple user interface allows players to select a
  standard game or to enter a custom FEN position and to select a color before
  challenging the engine to a game.  The codebase has additional functionality to
  interface with the Lichess API to play online.

Performance:
  The latest cversion (commit: TODO) has an average branching factor of 6.TODO and reaches
  a depth of 7 plies in an average of 2.TODO seconds. The engine has achieved an online
  rating of 1300 ELO (Lichess handle: BOT KTYBX-2025)
 
Structure:
  Built on bitboard piece representation and move generation, with a variety of arrays
  that record capture sequences and evaluation history. The core search engine is an
  iterative-deepening negamax algorithm with quiescence search. Additional structures
  including a hashing table, heuristic move ordering, and late move reduction are also
  implemented.

About Bitboards:
  An obvious approach to a chess engine might involve an 8 x 8 array with each element
  being a character corresponding to pieces on the board. However, this method is slow due
  to repetitively having to look up array entries and implement if-structures to evaluate
  positions. Bitboards, on the other hand, are 64-digit binary numbers corresponding to each
  piece, which enable arithmetic operations to be employed between bitboards for move
  generation, board evaluation, etc.

Game Theory Discussion:
  The core negamax algorithm is equivalent to the better-known minimax algorithm. Minimax
  is a powerful game theory algorithm for two-player, perfect information games which works
  by maximizing the board evaluation for one player and minimizing the board evaluation for
  the other player, while taking turns making moves for either side. It is a so-called
  "depth first" search, meaning it traverses each branch in the move tree to the leaf node,
  where a scoring function is implemented to evaluate the preceding branch. Negamax simplifies
  minimax by eliminating the minimizing player - the evaluation is negated between each
  ply so that the side to play is always maximizing.

About Alpha-Beta Pruning:
  Minimax is theoretically a perfect search algorithm. However, the majority of moves in
  any given search tree are irrelevant. The search tree is improved by pruning branches
  that are guaranteed to not improve the outcome of the search, allowing a deeper search
  to be done on more relevant moves in the same amount of time. The standard way to do this
  is by tracking the best evaluations obtained by both the maximizing and minimizing player
  (as alpha and beta). Then, if a new evaluation is found at a depth exceeding the limit
  from the previous depth, the rest of that sub tree is pruned.  This is completely
  equivalent to minimax.

About Transposition Tables
  To further improve search efficiency, the evaluation for every position is stored in a
  transposition (hash) table so that if the same position occurs in a different branch of
  the game tree, it isn't searched again.

Move Ordering:
  Because making alpha-beta cut-offs depends on having previously established good
  continuations for each side, searching good moves first significantly increases the
  number of branches that can be pruned. In a minimax search engine, move ordering
  is the single most important factor for reaching greater depths.  It is a bit
  paradoxical to find the best move by searching for the best move first, but there are
  generally good tools for establishing a strong move order. Kitty Box uses many
  simple heuristics as well as iterative deepening which are described below:

 *Heuristics - Simple rules of thumb are generally good enough to establish a strong
  move order.  These rules can include looking at captures before other moves, looking
  at moves that control the center, looking at moves that triggered alpha-beta
  cut offs in the previous branch, and so on.

 *Iterative Deepening - Instead of immediately searching the move tree at full depth,
  it is beneficial to search at shallower depths first and to use those results to improve
  the move order. Because the move tree gets exponentially wider with each depth,
  searching at a shallower depth adds only a tiny fraction of calculation time to the
  overall search, making this technique extremely powerful.  In fact, it is so powerful
  that incrementally working up to a full depth search, each time using the results
  of shallower searches to improve the move order can actually save time.

About Late Move Reduction:
  Because the engine generates a very strong move order for each search, moves at the
  end of the list can be searched to a reduced depth. The first 30% of legal moves are
  searched at full depth. The last 25% percent of moves are searched at 1 below full
  depth and the remaining moves are searched at 3 below full depth. This is aggressive
  and is bound to miss some good moves but absolutely necessary for a serious depth.

About Quiescence Search:
  Accurate evaluation of leaf nodes is critical for minimax to work well. If a leaf
  node occurs in the middle of a tactical sequence where one side is momentarily up in
  material, then the engine may get a flawed sense of the value of that node.  This can
  be mitigated to a certain extent by deeper searches (e.g. a leaf node spotted ten moves
  in the future will have a small impact on the overall evaluation). Even so, it is
  beneficial to continue searching deeper until the position is relatively calm before
  scoring it.  This is known as a quiescence search.  Unlike alpha-beta pruning, it
  takes away valuable time from the search, but it significantly improves engine
  evaluation, so it is generally worth performing.

About Evaluation:
  Kitty Box uses a combination of material advantage and positional advantage to score
  calm leaf nodes.  Material advantage is calculated based on a point scale initialized
  in the Engine settings section of the source code.  Positional advantage is derived
  from a series of piece value tables which gives each square a score depending on what
  piece is there.  The sum of these two (with material being weighted slightly more) is
  returned as the evaluation for a node.