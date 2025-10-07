# Mini UNIX Shell — CS370 Operating Systems (Fall 2024)

This repository contains my implementation of a minimal UNIX-like command-line interpreter (shell) for the **CS370: Operating Systems** course at **LUMS**, taught by **Dr. Naveed Anwar Bhatti, Dr. Muhammad Hamad Alizai, and Dr. Momina Khan**.

This project replicates the **core functionalities of a UNIX shell** (such as Bash or Zsh), providing hands-on experience in systems programming and deepening understanding of process creation, I/O redirection, and inter-process communication.  

---

## Implemented Features

This shell implements the following key components from the assignment specification:

- **Interactive and Script Mode:** Supports both user-interactive commands and file-based execution.
- **Built-in Commands:** Includes core internal commands (`cd`, `pwd`, `pushd`, `popd`, `dirs`, `alias`, `unalias`, `echo`, and `exit`).
- **Executable Commands:** Executes system-level programs via `fork()` and `execvp()`.
- **Command Chaining:** Supports logical and sequential chaining operators (`&&`, `||`, `;`).
- **Pipes and I/O Redirection:** Implements inter-process communication using `|`, `>`, `>>`, and `<`.
- **Combined Pipelines:** Handles chained pipelines with mixed built-in and external commands. 
