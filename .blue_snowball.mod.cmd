savedcmd_blue_snowball.mod := printf '%s\n'   blue_snowball.o | awk '!x[$$0]++ { print("./"$$0) }' > blue_snowball.mod
