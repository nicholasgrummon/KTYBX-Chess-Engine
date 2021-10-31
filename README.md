# KTYBX-Chess-Engine

KITTY BOX COMMAND-LINE CHESS ENGINE
Kitty Box is a chess engine made from scratch in C++

Kitty Box Project Goals:
  The goal of Kitty Box is to create a strong chess engine that is easily understandable
  and is built completely from scratch.  It is very important to me to keep my code as
  simple as possible and to limit the use of thrid party libraries and modules because I
  want Kitty Box to be something you can enjoy without a high level of coding experience.

Kitty Box Features:
  Kitty Box is a command line chess engine that uses algebraic notation and ASCII
  graphics to play chess.  A simple user interface allows players to select a
  standard game or enter a custom FEN position and to select a color before
  challenging the engine to a game.  Kitty Box uses bitboard piece representation
  and a variety of tables to generate moves and facilitate the move search.  The
  core search engine is an iterative-deepening negamax algorithm with alpha-beta
  pruning.  A quiescence search is also used to make sure no tactics are missed as
  well as a transposition table to avoid analyzing the same position more than once and
  many additional heuristics go into improving move ordering.

Kitty Box Performance:
  Right now, Kitty Box has an average branching factor of around 6 and reaches a depth of
  7 or 8 plies in a couple seconds.  The goal is to refine the move ordering and move search
  algorithms to reach a branching factor of around 4 and to quickly reach a depth of over
  12 plies and/or reach around 1800 ELO.

  Lines of functional code (excluding comments and non-essential UI): ~1100
  ELO Rating: ~1300

About Bitboards:
  An obvious approach to a chess engine might involve an 8 x 8 array with each element
  being a "char" corresponding to pieces on the board.  In fact, my earliest attempts at
  a chess program were exactly this.  However, this method is slow due to repetitively
  having to look up array entries and is conducive to using lots of if-statements to
  evaluate positions.  A better approach is to use bitboards. Bitboards are just long
  binary numbers with each digit corresponding to a square. A chess board has 64
  squares and 12 different types of pieces, so you need at least twelve 64 bit numbers
  to fully represent a chess board.  At first, this seems a lot more complicated than
  using arrays, but the power of bitboards comes from the fact that they are just
  numbers and as a result you can do math with them. Computers are built to do math,
  so even though bitboards seem more complicated than arrays, they are much easier
  for the computer to work with and therefore much faster.

Board Representation:
  Kitty Box still uses an array to store the bitboards, but it is more of a look-up
  table for finding the bitboard with a certain value rather than a full board
  representation. Perhaps having an array keep track of the position defeats the
  whole purpose of using bitboards, but it is, at least for now, the best option.
  You simply have to have some way to combine piece bitboards into a complete position
  because it is necessary to be able to modify the bitboard you want without knowing
  in advance which one it is. This requires at the very minimum an updated list of
  bitboards. The perfect solution would be to have one single bitboard that is 
  large enough to contain all the necessary information about a position. This is
  possible for a tic-tac-toe program, but for chess that would require at least a 960
  bit number which, aside from being completely impossible, would overwhelm any
  theoretical gain in efficiency by its sheer massiveness.  Thus, the only real 
  options are a class, a pointer system, or an array.  I have tried each of
  these, and an array is by far the most effective solution.  Ultimately, though, 
  board representation doesn't matter because the primary gain in efficiency
  provided by bitboards comes from the move generation process which is
  unaffected by this array.  Maybe if I was interested in making a world class
  chess engine, I would find a marginally better way to store positions, but that is
  not the goal of Kitty Box. I just want to create a decently strong, but most
  importantly an easily understandable chess engine built from scratch. This is
  probably the weakest structural part of Kitty Box, but it is still a more than
  sufficient architecture for a strong engine.

About Minimax and Negamax:
  the Kitty Box engine is built on a core negamax algorithm which is a version of
  minimax.  Minimax is a powerful algorithm for two-player, perfect information games.
  It works by trying to maximize the board evaluation for one player and to minimize
  the board evaluation for the other player by taking turns making moves for each side.
  It is a so called depth-first search, meaning it travels from the "root" position
  along a branch in the move tree down to a certain depth (called the leaf node). There, 
  an evaluation function scores the position making it possible for the algorithm to
  determine the strength of the moves on the branch leading to this node.  After
  evaluating every branch, the algorithm can choose the best move to play in the root
  position.  Negamax simplifies minimax by eliminating the minimizing player.  Instead
  of keeping an absolute evaluation where a positive score is good for the maximizing
  player and negative score is good for the minimizing player, the negamax algorithm
  negates the evaluation between each depth so that the player whose turn it
  is is always maximizing, hence nega-max.  This is completely equivalent to minimax;
  it simply makes the code shorter and easier to work with.

About Alpha-Beta Pruning:
  Minimax is theoretically a perfect search algorithm, meaning that if it searches
  every branch to an infinite depth, it is guaranteed to find the best move. However,
  searching every branch is highly inefficient because most moves, even if legally
  playable, are irrelevant and only waste time to prove they are bad. Thus minimax can
  be improved by pruning branches that are guaranteed to not improve the outcome of the
  search, allowing a deeper search to be done on more relevant moves in the same amount
  of time. The standard way to do this is to track the best evaluations obtained by
  both the maximizing and minimizing player as alpha and beta and if a new evaluation
  is found at a certain depth that exceeds the limit from the previous depth, then the
  rest of that sub tree does not need to be searched because a sufficient move has
  already been found.  This is completely equivalent to minimax; it simply improves
  speed.

About Transposition Tables
  To further improve search efficiency, the evaluation for every position is stored in a
  transposition table so that if that position occurs in another branch of the game tree
  it doesn't need to be searched again.

About Move Ordering:
  Because making alpha-beta cut offs depends on having previously established good
  continuations for each side, searching good moves first significantly increases the
  number of branches that can be pruned. In a minimax search engine, move ordering
  is the single most important factor for reaching greater depths.  It is a bit
  paradoxical to find the best move by searching for the best move first, but there are
  generally some good tools for establishing a strong move order. Kitty Box uses many
  simple heuristics as well as iterative deepening which are all described below.

 *Heuristics - Simple rules of thumb are generally good enough to establish a strong
  move order.  These rules can include looking at captures before other moves, looking
  at moves that control the center, looking at moves that triggered alpha-beta
  cut offs in the previous branch, and so on.

 *Iterative Deepening - Instead of immediately searching the move tree at full depth,
  it is beneficial to search at shallower depths first and use those results to improve
  the move order. Because the move tree gets exponentially wider with each depth,
  searching at a shallower depth adds only a tiny fraction of calculation to the
  overall search, making this technique extremely powerful.  In fact, it is so powerful
  that incrementally working up to a full depth search, each time using the results
  of shallower searches to improve the move order can actually save time.

About Late Move Reduction:
  Because the engine generates a very strong move order for each search, moves at the
  end of the list can be searched to a reduced depth. The first 30% of legal moves are
  searched at full depth. The last 25% percent of moves are searched at 1 below full
  depth and the remaining moves are searched at 3 below full depth. This is aggresive
  and bound to miss some good moves but absolutely necessary for a seriously deep search.

About Quiescence Search:
  Accurate evaluation of leaf nodes is critical for minimax to work well. For instance,
  if a leaf node occurs in the middle of a tactical sequence where one side is
  momentarily up in material, then the engine may get a flawed sense of the value of
  that node.  This can be mitigated to a certain extent by deeper searches (e.g. a leaf
  node spotted ten moves in the future will have less of an impact on the overall
  evaluation than a leaf node spotted only five moves in the future). Even so, it is
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

Kitty Box Features:
  In addition to the minimax chess engine itself, Kitty Box has an extensive user
  interface. The program can read user input FEN positions, give hints, and much more.

Developed by Nicholas Grummon c 2020 - 2021
