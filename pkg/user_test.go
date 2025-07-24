package pkg

import (
	"os/user"
	"testing"
)

func TestValidateUser(t *testing.T) {
	tests := []struct {
		name     string
		username string
		wantErr  bool
	}{
		{
			name:     "empty username should return current user",
			username: "",
			wantErr:  false,
		},
		{
			name:     "current user should be valid",
			username: getCurrentUsername(t),
			wantErr:  false,
		},
		{
			name:     "nonexistent user should return error",
			username: "nonexistent_user_12345",
			wantErr:  true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := ValidateUser(tt.username)
			if tt.wantErr {
				if err == nil {
					t.Errorf("ValidateUser() expected error but got none")
				}
			} else {
				if err != nil {
					t.Errorf("ValidateUser() unexpected error: %v", err)
				}
				if result == "" {
					t.Errorf("ValidateUser() returned empty username")
				}
			}
		})
	}
}

func TestGetCurrentUser(t *testing.T) {
	user, err := GetCurrentUser()
	if err != nil {
		t.Errorf("GetCurrentUser() unexpected error: %v", err)
	}
	if user == nil {
		t.Errorf("GetCurrentUser() returned nil user")
	}
	if user.Username == "" {
		t.Errorf("GetCurrentUser() returned user with empty username")
	}
}

func TestUserExists(t *testing.T) {
	tests := []struct {
		name     string
		username string
		want     bool
	}{
		{
			name:     "current user should exist",
			username: getCurrentUsername(t),
			want:     true,
		},
		{
			name:     "nonexistent user should not exist",
			username: "nonexistent_user_12345",
			want:     false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := UserExists(tt.username); got != tt.want {
				t.Errorf("UserExists() = %v, want %v", got, tt.want)
			}
		})
	}
}

// Helper function to get current username for tests
func getCurrentUsername(t *testing.T) string {
	t.Helper()
	user, err := user.Current()
	if err != nil {
		t.Fatalf("Failed to get current user: %v", err)
	}
	return user.Username
}
