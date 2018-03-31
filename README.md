
### OGS score estimator Benchmark 

Tools to test various ogs score estimators on a sgf collection of finished games.

Right now it supports:  
* [ogs](https://github.com/online-go/score-estimator)
* [gnugo](https://github.com/lemonsqueeze/score-estimator/tree/gnugo)
* [pachi](https://github.com/lemonsqueeze/pachi/tree/ogs_estimator)

But should work with any estimator that conforms to ogs score estimator format. 

**Build**

'make' first to build.

Get the estimators with `estimators/get_estimators` (will clone and build each one)

**Scoring games**

    cd sgf/9x9
    ../../bin/score_games ogs
    ../../bin/score_games pachi
    ../../bin/score_games gnugo

Estimator output is saved in eg 'file.gnugo.raw' for each sgf.  
'file.gnugo' is the post-processed file with official game score
(taking komi, handicap etc into account) and final ownermap.

To compare estimators results with sgf scores:

    ../../bin/game_infos         # once
    ../../bin/compare_scores ogs
    ../../bin/compare_scores pachi
    ../../bin/compare_scores gnugo

**SGF database**

The games come from [ogs archive](https://github.com/gto76/online-go-games)
after running `bin/filter_games` to keep only finished/suitable games
and fixing the ones that were scored incorrectly.

