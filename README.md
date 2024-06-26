# Fuzzing Maven-built C++ code with Mayhem

A big piece of the code analysis puzzle is integrating with
build systems, and this can be hard when using non-standard tools.  Within the
C/C++ community we typically see systems like `make`, `cmake`, and `ninja`.  We
were recently asked to help with a code base that used Maven, which is
typically used for Java.

The project was a large code base, split into multiple different repositories.
We'll simplify a bit here and assume just two repositories: 

* `mylibrary`: A maven project for building a shared library. This builds as a
  static (`.a`) library. 
* `myapp`: A maven project that uses `mylibrary` to build a self-contained app.
* `myharness`: A maven project that uses `mylibrary` to build a harness that Mayhem can use.

Let's dive into how we did it, and how you can too. You can also check out our
[Github repo](https://github.com/dbrumley/maven-example) showing a
full working example.


## Dependency Hell and Maven to the Rescue

Every developer knows about "dependency hell." You might be experiencing
dependency hell when your software has: 

1. **Complex Dependency Trees**: Software projects often rely on numerous
   libraries, each of which may have its own set of dependencies (transitive 
   dependencies). This creates a complex tree of dependencies that can be 
   difficult to resolve and manage. 
   
2. **Version Conflicts**: Different parts of a project or different projects on
   the same system might require different versions of the same dependency.
   Managing these version conflicts manually can be challenging and
   time-consuming. 
   
3. **Lack of Isolation**: In `./configure && make` scenarios, dependencies are
   typically installed system-wide. This lack of isolation can lead to
   conflicts between projects, where one project's dependencies interfere with
   another's. 

4. **Manual Resolution**: Developers often have to manually resolve
   dependencies, determining which libraries and versions are required, and
   ensuring they are correctly installed. This process can be error-prone and
   tedious, especially for large projects with many dependencies. 


### What is Maven?

Maven helps avoid some of these problems. While non-standard in C/C++, it
seemed to work out nicely for this customers use case. Maven's primary use case
is Java, where it's well known for its project management, dependency
management, and build lifecycle capabilities. Maven uses a Project Object
Model (POM) in an XML file (`pom.xml`) to describe the project configuration,
dependencies, build order, and required plugins. Each project has a unique 
combination of `groupId` and `artifactId` which is used to reference the project
as a dependency.

This project chose Maven to help avoid some dependency hell issues. In
particular, three were important to understand for integrating new tests:

* **Centralized Repository**: Maven is designed around centralized binary
  repositories. The default `https://mvnrepository.com` hosts terabytes of
  built open source projects. An organization can host their own internal
  repository as well, so Maven can work in environments that don't have public
  internet access.

* **Local Repository**: Maven caches all downloaded dependencies in a local
  repository on your machine. By default this is in the `.m2` directory in your home folder
  (e.g., `~/.m2/repository` on Unix-like systems, or via the `M2_HOME`
  environment variable). 

* **Explicit Dependency Management**: Dependencies are defined in the project's
  `pom.xml` file, and Maven automatically downloads them from the repository.
  This reduces the need for manual management of libraries and their versions.  


### Maven Plugins for C/C++

In order to compile C/C++ code with Maven, you need to understand its plugin
ecosystem. There are three ways to do this:

- **Maven with NAR for C++**: If you're using Maven with the NAR plugin for C++
  projects, Maven handles the overall project management, dependency
  resolution, and invokes the NAR plugin for the C++ specific build steps like
  compilation and linking. 
  
- **Ant for C++ Builds**: If you prefer Ant, you can use it independently for
  C++ projects, scripting the entire build process in the `build.xml` file.
  This approach is more manual compared to Maven but offers flexibility. 
  
- **Combining Maven and Ant**: In some complex scenarios, you might see Maven
  and Ant being used together. For instance, Maven could be used for its
  superior dependency management and project structuring, while Ant scripts are
  invoked for specific tasks within the Maven build lifecycle that require
  custom handling not easily achieved with Maven plugins. 

In this project we focused on integrating directly with Maven + NAR. 


## Step 1: Build the project as-is

When faced with new software, a good first step is always to make sure you know
how to compile it. The Maven CLI `mvn` manages the overall process:

* **`mvn compile`** compiles the project, and puts the results (by default) in
  the `targets` directory.
* **`mvn install`** will compile and install the project into the local Maven
  repository, by default in `~/.m2`.

Many other steps exist, but these two suffice for our purposes. For more details, check 
the [Maven
lifecycle](https://maven.apache.org/guides/introduction/introduction-to-the-lifecycle.html).

In this project, the following will happen:
 - `myapp` will build to a native executable in
   `./myapp/target/nar/myapp-1.0-SNAPSHOT-amd64-Linux-gpp-executable/bin/amd64-Linux-gpp/myapp`.
   Similarly for the other directories.
- The `install` command will install all components as NAR files in `$HOME/.m2/repository/com/forallsecure/myapp/1.0-SNAPSHOT/` 


## Step 2: Decide how you will integrate new test harnesses

Dynamic analysis typically requires, like unit and functional tests, building a
test harness. A test harness includes code for mocks, test drivers, and any
setup that needs to be done before performing the actual test.

There are two ways to do this:
1. Create a new Maven project with just the harnesses that import the system under
    test via Maven dependency management. This typically works great for libraries where
    the harness can use the unmodified objects being built as-is.
2. Sometimes, code you want to test is contained in an executable (not a library). In 
    this case, new harness code would need to be added directly to the Maven module that contains 
    the system under test. If you can't add code to the central repo, a common approach is to 
    maintain a fork with your local additions. We took this approach.

In this case, the interface the library exports suffices and we'll do (1).

### Step 3: Create a `myharness` module.

Maven has a very opinionated default directory structure.  It expects code in
`src/main/cpp` for C++ code, includes in `src/main/include`, and tests in
`src/test/cpp`.  While you can override these, it's often easier just to work
with defaults.  In our case, we're going to add the harness code to the default
`src/main/cpp` in a new Maven project.

We also create a new `pom.xml` in the `myharness` module and add the `myharness` 
module to the parent `pom.xml`.

### Step 4: Write and Run Your Harness.

With this all figured out, we added a new file `src/main/cpp/main.c` as our
harness. The harness needs to define `main`, and in our case it was a simple
call to test the `CelsiusToFahrenheit` function.

You can compile this harness with `./mvnw install` (local Maven) `docker build --platform=linux/amd64 .` (dockerized Maven).

## Running with Mayhem

Performing dynamic analysis with Mayhem is super easy after the harness is
built. Just push the docker image, and specify a `Mayhemfile` saying how to
check the application:

Build the Docker image so C++ can be compiled
```bash
docker build --platform=linux/amd64 . -t dbrumley/maven-example:latest
docker push $MAYHEM_REPOSITORY/dbrumley/maven-example:latest
```

And here is the `Mayhemfile`:

```yaml
image: $MAYHEM_REPOSITORY/dbrumley/maven-example:latest
duration: 30
project: maven-example
target: harness1
cmds:
  - cmd: /myharness @@
```

Mayhem finds a bug almost instantly.  The example `conversion` function converts from a
double to a string, but doesn't do a bounds check properly (recall we pass in a `char
buf[...]` buf)
```c
void conversion(celsius_t c, kelvin_t k, char *buf)
{
  sprintf(buf, "%lf celsius is %lf kelvin", c, k);
  return;
}
```

Mayhem finds an example input that crashes the program almost immediately:
```bash
-5486128870225019196831937724308948771408482030063034011714200761338748526848860953068154467189167438254023901036469529976518899427116188186866388367687561101259543253404734162913518640480693972842919781152860573843336048011515106063717714234487990508433623412693986670877298875553313862829328459030855680.000000 celsius is -9875031966405034554297487903756107788535267654113461221085561370409747348327949715522678040940501388857243021865645153957734018968809138736359499061837609982267177856128521493244333552865249151117255606075149032918004886420727190914691885622078382915180522142849176007579137975995964953092791226255540224.000000 kelvin
Segmentation fault

```

### Bonus: Unit testing your harness (running tests with the nar plugin)

We can add the added Mayhem test coverage so that it runs under Maven as well
as a unit test.  However, figuring out what to modify in the build is tricky,
especially in figuring out how to modify `pom.xml`.  We finally found a [great
writeup](https://groups.google.com/g/maven-nar/c/-XSwh3l47Ow) and example from
Jef Douglas on the NAR mailing list.  He even put together a [Github
repo](https://github.com/dugilos/nar-test) demonstrating the idea.

The key part for us was to understand that the test name in the `pom.xml` file
are related to how you name your tests.  If you have the test named `test1`, by
default this will refer to `src/test/cpp/test1.cpp`.  

Jef gives a wonderful example. Suppose you have the configuration:
```xml
<configuration>

    <tests>

        <test>
            <name>test1</name>
            <link>shared</link>
            <run>true</run>
        </test>

        <test>
            <name>test2</name>
            <link>shared</link>
            <run>true</run>
            <args>
                <arg>param1</arg>
                <arg>param2</arg>
            </args>
        </test>

    </tests>

</configuration>
```

The configuration above will build 2 executables, `test1` and `test2`, each
executable will be built with all the test source files whose names don't
correspond to the names of other tests. 

You can add other files as well, as long as they don't conflict with the test
names. If you have 4 files in `src/test/cpp/{main.c, util.c, test1.c,
test2.c}` the executable `test1` will be built from `main.c`, `util.c`,  and
`test1.c`, and  the executable `test2` will be built from `main.c`,
`util.c`, and `test2.c`.  

In our case we're just writing one harness. The key is to make sure the source
file name matches the name given here. We'll use `harness1`, and create `harness1.c`:

```
       <tests>
            <test>
              <name>harness1</name> <!-- Set your test executable name here -->
              <link>static</link>
              <run>true</run>
              <args>
                <arg>${project.basedir}/src/test/resources/42.test</arg>
              </args>
            </test>
          </tests>
```



## Conclusion

Figuring out Maven was a fun journey.  We've created a small example
illustrating everything at [](https://github.com/dbrumley/maven-example)
including:
1. How to compile multiple Maven projects
2. How to [specify a dependency between the two Maven projects](https://github.com/dbrumley/maven-example/blob/7c333d4a3230fd35ba5ce4b715cd86728b464026/myapp/pom.xml#L14), in this case
   `myapp` using `mylibrary` and `myharness` using `mylibrary`. 
3. Writing a harness and including it in the [build file](https://github.com/dbrumley/maven-example/pom.xml).. 
4. Testing with Mayhem, which only requires writing a simple configuration
   file. The result is immediate bug-finding Mayhem making!   
   
   

## Appendix: Other Notes

### Q: How do you build just one subproject?

A: Suppose you have in your `pom.xml` a list of projects with `myapp` as one of
them. Run `mvn --pl myapp compile`

### Q: How do you invoke NAR to compile directly?

A: Using a NAR goal, such as:  `mvn compile`.  If you want to specifically
invoke the NAR target, `mvn
com.github.maven-nar:nar-maven-plugin:3.3.0:nar-compile`.

### Q: I see installed a .nar file; how do I get the actual code?

A: In a target that uses it, run: `mvn
com.github.maven-nar:nar-maven-plugin:3.3.0:nar-unpack`. This will unpack the
installed dependency nar file into `target`

### Q: How do I output a logfile?

A: Run `mvn -l <logfile> <cmd>` to output to `<logfile>`.  Colors are disabled. 

### Q: How do I change to clang?

A: There are two ways. If `pom.xml` has a property like
<cxx.compiler>g++</cxx.compiler>, you can override it by running Maven with:

```bash
mvn compile -Dcxx.compiler=clang++
```

If uses env variables, try:

```bash
export CXX=clang++
mvn compile
```

One could also create a new profile and use that profile flags, e.g., for the `clang-profile`:

```bash
mvn compile -Pclang-profile
```

### Unsorted

When starting out with an existing repo, here is a general workflow:

1. Configure the `MAVEN_HOME` and `PATH` environment variables.
2. find `pom.xml` and the `<modules>` section.
3. Determine which `<build><plugins>` are being used for C++.
4. Full build: `mvn clean install`
5. if `pom.xml` has a property like <cxx.compiler>g++</cxx.compiler>, you can override it by running Maven with:
```bash
mvn compile -Dcxx.compiler=clang++
```
If uses env variables, try:
```bash
export CXX=clang++
mvn compile
```


6. To run wrappers, e.g., `./mvnw clean install`

