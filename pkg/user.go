package pkg

import (
	"errors"
	"os/user"
)

// ValidateUser validates a username and returns the validated username.
// If username is empty, it returns the current user's username.
// If username is provided, it validates that the user exists.
func ValidateUser(username string) (string, error) {
	if username == "" {
		currentUser, err := user.Current()
		if err != nil {
			return "", errors.New("current user check failed")
		}
		return currentUser.Username, nil
	}

	_, err := user.Lookup(username)
	if err != nil {
		return "", errors.New("user does not exist")
	}

	return username, nil
}

// GetCurrentUser returns the current user information
func GetCurrentUser() (*user.User, error) {
	return user.Current()
}

// UserExists checks if a user exists in the system
func UserExists(username string) bool {
	_, err := user.Lookup(username)
	return err == nil
}
