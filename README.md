# Fuzzing Maven-built C++ code with Mayhem

We recently were asked about integrating Mayhem analysis into a C/C++ project, but with a twist. Normally C/C++ projects used `make`, `cmake`, or `ninja` as the build system.  The twist in this project was integrating into `maven`. 

The project was a large code base, split into multiple different repositories.  We'll simplify a bit here and assume just two repositories:

* `mylibrary`: A maven project for building a shared library.
* `myapp`: A maven project that uses `mylibrary` to build a self-contained app.

## Dependency Hell and Maven to the Rescue

When working with software projects that use the traditional build process of `./configure && make`, developers often encounter a challenge commonly known as "dependency hell." This term refers to the difficulties and complications that arise when managing and resolving the dependencies required for building and running software.

Some characteristics of dependency hell include:

1. **Complex Dependency Trees**: Software projects often rely on numerous libraries, each of which may have its own set of dependencies. This creates a complex tree of dependencies that can be difficult to resolve and manage.
    
2. **Version Conflicts**: Different parts of a project or different projects on the same system might require different versions of the same dependency. Managing these version conflicts manually can be challenging and time-consuming.
    
3. **Lack of Isolation**: In `./configure && make` scenarios, dependencies are typically installed system-wide. This lack of isolation can lead to conflicts between projects, where one project's dependencies interfere with another's.
    
4. **Manual Resolution**: Developers often have to manually resolve dependencies, determining which libraries and versions are required, and ensuring they are correctly installed. This process can be error-prone and tedious, especially for large projects with many dependencies.

Maven helps avoid some of these problems. While non-standard in C/C++, it seemed to work out nicely for this customers use case.

### What is Maven?

Maven is a build automation tool primarily used for Java projects. It's known for its project management, dependency management, and build lifecycle capabilities. Maven uses a Project Object Model (POM) in an XML file (`pom.xml`) to describe the project configuration, dependencies, build order, and required plugins.

Maven can help avoid some dependency hell issues such as:
* **Local Repository**: Maven stores all downloaded dependencies in a local repository on your machine. This isolates projects from one another, reducing conflicts and ensuring that dependencies for one project do not interfere with another. By default this is in the `.m2` directory in your home folder (e.g., `~/.m2/repository` on Unix-like systems). Think of this is like a local namespace.
* **Explicit Dependency Management**: Dependencies are defined in the project's `pom.xml` file, and Maven automatically downloads them from the repository. This reduces the need for manual management of libraries and their versions.

For C++ projects, Maven is not the standard tool, as it's inherently designed for Java. The final piece of the puzzle was to understand how Maven was applied to C/C++.  The project used the  NAR (Native ARchive) Maven plugin.


### NAR Maven Plugin
The NAR plugin extends Maven's capabilities to C++ projects. It allows Maven to compile, test, and package native code (like C++). This plugin is what makes it possible to use Maven, which is traditionally a Java tool, with C++ projects. The NAR plugin handles:

- Compilation of C++ source code.
- Management of C++ dependencies.
- Packaging of the compiled code into various formats (e.g., static libraries, shared libraries, executables).

When you use Maven with the NAR plugin for a C++ project, you still define your project configuration in a `pom.xml` file, but the plugin adds the capability to understand and process C++ specific build steps.

### Ant

This was a fairly large code base that had matured over time. The project also had several `ant` artifacts, 

Ant is another build automation tool, but unlike Maven, it's more flexible in terms of the types of projects it can handle. Ant is used to script custom build processes and is often favored for its simplicity and flexibility. Ant uses XML-based build files (`build.xml`) to describe the build tasks and their dependencies.

In C++ projects, Ant can be used to:

- Compile C++ source code.
- Link object files and libraries.
- Manage pre-build and post-build tasks.

### Putting it all together: Relationship and Usage in C++ Projects

- **Maven with NAR for C++**: If you're using Maven with the NAR plugin for C++ projects, Maven handles the overall project management, dependency resolution, and invokes the NAR plugin for the C++ specific build steps like compilation and linking.
    
- **Ant for C++ Builds**: If you prefer Ant, you can use it independently for C++ projects, scripting the entire build process in the `build.xml` file. This approach is more manual compared to Maven but offers flexibility.
    
- **Combining Maven and Ant**: In some complex scenarios, you might see Maven and Ant being used together. For instance, Maven could be used for its superior dependency management and project structuring, while Ant scripts are invoked for specific tasks within the Maven build lifecycle that require custom handling not easily achieved with Maven plugins.

## Building the project

The first step I always perform before integrating a new technology like Mayhem is to make sure I can reproduce the build process.

It was pretty simple here:
```
# Change to the library repository
cd mylibrary

# Install all dependencies, build the library, and install in the maven local repository
mvn install

# Change to the app repository
cd ../myapp

# Compile the app. No need to install.
mvn compile
```

The build system, by default, does a lot to manage where build artifacts are placed.  By default, things are placed in the `target` directory:
```
ls -ltar target/nar/myapp-1.0-SNAPSHOT-amd64-Linux-gpp-executable/bin/amd64-Linux-gpp/myapp 

-rwxr-xr-x 1 root root 16312 Jan  3 22:59 target/nar/myapp-1.0-SNAPSHOT-amd64-Linux-gpp-executable/bin/amd64-Linux-gpp/myapp
```

## Integrating Mayhem as a separate repo

The next step is to integrate Mayhem, and the process is the same as adding new tests and testing mocks.  In dynamic analysis, we call this "harnessing".  The harness code is added to the source code repo.

In this case, the security team didn't have write access to the code repositories. There are two common ways we advise clients to solve this issue (assuming they can't just restructure their organization):
1.  Copy the code to a new git repo, and add the local harnesses there. 
2. Create a new repository with just the harnesses

Both work. In this case we'll do (1). It involves  creating test harness source files and configuring Maven to recognize and run these tests.

### Step 1: Define a New Profile for Fuzz Testing

In your `pom.xml`, you can define a new profile specifically for fuzz testing. Profiles in Maven allow you to customize your build for different environments or purposes.

```
<project>
    <!-- Existing project configuration -->

    <profiles>
        <profile>
            <id>fuzz-tests</id>
            <build>
                <plugins>
                    <plugin>
                        <groupId>com.github.maven-nar</groupId>
                        <artifactId>nar-maven-plugin</artifactId>
                        <version>[Latest NAR Plugin Version]</version>
                        <extensions>true</extensions>
                        <configuration>
                            <!-- Configuration specific to fuzz testing -->
                            <testSourceDirectory>${project.basedir}/src/mayhem/cpp</testSourceDirectory>

                            <tests>
                                <test>
                                    <name>myappfuzztest</name>
                                    <link>shared</link>
                                    <run>true</run>
                                </test>
                            </tests>
                        </configuration>
                    </plugin>
                    <!-- Any other plugins needed for fuzz testing -->
                </plugins>
            </build>
        </profile>
    </profiles>
</project>

```


### Step 2: Create the Mayhem test harness directory

First, adjust the directory structure of your `myapp` project to include a "mayhem" directory for your test files:
```
myapp/
├── src/
│   ├── main/
│   │   └── cpp/
│   │       └── ... # Your main application source files
│   └── mayhem/
│       └── cpp/
│           └── ... # Your test harness source files
└── pom.xml

```


### Step 3: Update `pom.xml` to Recognize the "mayhem" Directory

Modify the `pom.xml` of your `myapp` project to tell Maven and the NAR plugin to look for test sources in the "mayhem" directory (by default Maven testing uses the `test` directory)

```
<project>
    <!-- Existing project configuration -->

    <build>
        <plugins>
            <plugin>
                <groupId>com.github.maven-nar</groupId>
                <artifactId>nar-maven-plugin</artifactId>
                <version>[Latest NAR Plugin Version]</version>
                <extensions>true</extensions>
                <configuration>
                    <!-- Other configuration -->

                    <!-- Custom test source directory -->
                    <testSourceDirectory>${project.basedir}/src/mayhem/cpp</testSourceDirectory>

                    <tests>
                        <test>
                            <name>myappmayhemtest</name>
                            <link>shared</link>
                            <run>true</run> <!-- Set to true to run tests -->
                        </test>
                    </tests>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>

```

In this configuration, the `<testSourceDirectory>` tag is used to specify the location of your test sources.

### Step 4: Write and Run Your Tests

Place your test files in the `src/mayhem/cpp` directory and write your tests as you normally would. You can then run your tests using Maven:

```
mvn test -P fuzz-tests
```


THIS IS WHAT IS NOT WORKING!
