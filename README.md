
### Gnugo-based OGS Score Estimator

This uses gnugo to make an ogs [score estimator](https://github.com/online-go/score-estimator)
for scoring finished games.

One cool thing about this one, there's nothing specific about gnugo actually.
Any gtp engine which understands `final_status_list dead` command can be used instead
(some may not like having final_status_list dead called out-of-the-blue though,
normally this happens after a genmove where the engine passed).

'make' to build.

The 'estimator' script should behave like ogs score estimator (same input / output).

If gnugo isn't in /usr/games/gnugo you'll have to fix the script.

Usage: `estimator  file.game`

For example: `estimator test_games/1776378.game`

This is only for scoring finished games, don't try this on middle game positions !
