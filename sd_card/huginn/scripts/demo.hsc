# Huginn-RF demo script (.hsc). One command per line; # starts a comment.
# Commands: DELAY <ms> | PRINT <text> | LOG <text> | IR <path> | REPEAT <n> ... END
PRINT Huginn-RF script engine
LOG demo script started
DELAY 500
REPEAT 3
  PRINT tick...
  DELAY 400
END
# Example: blast a saved IR file (uncomment and point at your own .ir)
# IR /huginn/ir/tvbgone.ir
PRINT done
LOG demo script finished
