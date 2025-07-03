package internal

import (
	"os/user"
)

type JvmProcess struct {
	Pid int32
	Cmd string
	user.User
}
