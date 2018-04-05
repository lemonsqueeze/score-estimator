
### Gnugo-based OGS Score Estimator

This uses gnugo to make an ogs [score estimator](https://github.com/online-go/score-estimator)
for scoring finished games.

There's nothing specific about gnugo actually, any gtp engine which understands
`final_status_list dead` can be plugged in here.
(some may not like final_status_list being called right away if they expect to have
passed just before, but this is easy to work around). Once dead stones are known tromp-taylor
counting is used to get the final ownermap. 

'make' to build.

The 'estimator' script should behave like ogs score estimator (same input / output).

If gnugo isn't in /usr/games/gnugo you'll have to fix the script.

Usage: `estimator  file.game`

For example: `estimator test_games/1776378.game`

This is only for scoring finished games, don't try this on middle game positions !
