package testutil

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

type JavaProcess struct {
	Cmd      *exec.Cmd
	ClassDir string
	Class    string
}

func StartJavaProcess() (*JavaProcess, func(), error) {
	const className = "TestMain"
	const javaSource = `
public class TestMain {
    public static void main(String[] args) {
        try {
            Thread.sleep(60000);
        } catch (InterruptedException e) {
            // ignore
        }
    }
}
`
	tmpDir := os.TempDir()
	javaFile := filepath.Join(tmpDir, className+".java")
	classFile := filepath.Join(tmpDir, className+".class")

	if err := os.WriteFile(javaFile, []byte(javaSource), 0644); err != nil {
		return nil, nil, err
	}

	javacPath, err := exec.LookPath("javac")
	if err != nil {
		return nil, nil, err
	}
	cmdCompile := exec.Command(javacPath, javaFile)
	cmdCompile.Dir = tmpDir
	if _, err := cmdCompile.CombinedOutput(); err != nil {
		return nil, nil, err
	}

	javaPath, err := exec.LookPath("java")
	if err != nil {
		return nil, nil, err
	}
	cmdRun := exec.Command(
		javaPath,
		"-cp", tmpDir,
		className,
	)
	cmdRun.Dir = tmpDir
	if err := cmdRun.Start(); err != nil {
		return nil, nil, err
	}

	cleanup := func() {
		_ = cmdRun.Process.Kill()
		cmdRun.Wait()
		os.Remove(javaFile)
		os.Remove(classFile)
	}

	return &JavaProcess{
		Cmd:      cmdRun,
		ClassDir: tmpDir,
		Class:    className,
	}, cleanup, nil
}

// CreateSimpleJavaAgent creates a simple Java agent jar that supports both premain and agentmain loading mechanisms.
func CreateSimpleJavaAgent() (string, func(), error) {
	const agentClassName = "SimpleAgent"
	agentSource := `
import java.lang.instrument.Instrumentation;

public class ` + agentClassName + ` {
    public static void premain(String agentArgs, Instrumentation inst) {
        System.out.println("SimpleAgent loaded by premain");
    }
    public static void agentmain(String agentArgs, Instrumentation inst) {
        System.out.println("SimpleAgent attached by agentmain");
    }
}
`
	tmpDir := os.TempDir()
	javaFile := filepath.Join(tmpDir, agentClassName+".java")
	classFile := filepath.Join(tmpDir, agentClassName+".class")
	manifestFile := filepath.Join(tmpDir, "MANIFEST.MF")
	jarFile := filepath.Join(tmpDir, agentClassName+".jar")

	// Write Java source file
	if err := os.WriteFile(javaFile, []byte(agentSource), 0644); err != nil {
		return "", nil, err
	}

	// Compile Java source file
	javacPath, err := exec.LookPath("javac")
	if err != nil {
		return "", nil, err
	}
	cmdCompile := exec.Command(javacPath, javaFile)
	cmdCompile.Dir = tmpDir
	if out, err := cmdCompile.CombinedOutput(); err != nil {
		return "", nil, fmt.Errorf("javac error: %v, output: %s", err, string(out))
	}

	// Write MANIFEST.MF with both Premain-Class and Agent-Class
	manifestContent := "Manifest-Version: 1.0\nPremain-Class: " + agentClassName + "\nAgent-Class: " + agentClassName + "\n"
	if err := os.WriteFile(manifestFile, []byte(manifestContent), 0644); err != nil {
		return "", nil, err
	}

	// Create jar file
	jarPath, err := exec.LookPath("jar")
	if err != nil {
		return "", nil, err
	}
	cmdJar := exec.Command(jarPath, "cmf", manifestFile, jarFile, agentClassName+".class")
	cmdJar.Dir = tmpDir
	if out, err := cmdJar.CombinedOutput(); err != nil {
		return "", nil, fmt.Errorf("jar error: %v, output: %s", err, string(out))
	}

	cleanup := func() {
		os.Remove(javaFile)
		os.Remove(classFile)
		os.Remove(manifestFile)
		os.Remove(jarFile)
	}

	return jarFile, cleanup, nil
}
