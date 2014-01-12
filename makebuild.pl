system("make -B");
my $build = int(`type publish.bld`) + 1;
system("echo $build > publish.bld");
print "Build number $build done!";
