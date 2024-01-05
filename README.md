# Collaborative Processes in C: Search Project
This project has as its main objective to implement and evaluate the performance of collaborative task execution using multiple processes in a Unix environment.

## Project Description
The project implements a multiprocess version of the `grep` utility in C language, focused on efficient pattern search in large files. A shared set of processes (process pool) has been created to inspect the file efficiently, evaluating the optimal number of processes to execute this task on large files.

## Main Components of the Program
1. **Process Creation Mechanism:**
Development of a cycle that allows the parent to create child processes efficiently.

2. **Process Communication:**
Use of a message queue to facilitate communication between processes.

3. **Process Synchronization:**
Implementation of synchronization techniques to coordinate the search in the archive in an effective way.

4. **File Reading:**
Development of a read function that handles 8K bytes blocks and ensures the integrity of the lines read.

5. **Matching search:**
Use of the "regex.h" library to search for regular expression matches in the file.

## Project Documentation
A formal documentation of this program can be found in the `doc.pdf` file within this repository. This project was created as a project for a course at my university (Tecnológico de Costa Rica) called 'Principles of Operating Systems', which is the reason why the documentation is in Spanish 😊.

In the documentation, clear definitions of the data structures used are presented. Sections detailing the process creation mechanism, the communication between processes and the effective synchronization to coordinate the search in the archive. In addition, the documentation addresses the operation of the file reading and text matching. Lastly, the performance tests provide a quantitative assessment of the impact of the number of processes on program execution time and the result of the program execution.

## Made by
[Sebastián Bermúdez A.](https://github.com/Sebasberacu/) and 
[Felipe Obando A.](https://github.com/Huevaldinho)
