use File::stat;
use Time::localtime;

my $build    = sprintf "%03d", int(`type publish.bld`);
my $pwd      = `cd`;

my $target   = "C:\\Far\\Plugins\\BlockPro\\Src.zip";
my $wwwhome  = "D:\\Andy\\WWW";
my $wwwtarget= "$wwwhome\\download\\blockpro.zip";

my @MONTHS   = qw(JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC);

chop($pwd);
system("makerelease.bat");
chdir("C:\\Far\\Dev");
unlink $target if -f $target;
system("zip $target -\@ <$pwd\\publish.lst >nul");
chdir("C:\\Far\\Plugins\\BlockPro");

exit unless -f $wwwtarget;
&UpdateReadme("Doc\\readme.txt");
unlink($wwwtarget);
system("zip -R -o $wwwtarget *.* >nul");
&UpdateSize("$wwwhome\\farplugins.html");
system("copy Doc\\readme.txt $wwwhome\\download\\blockpro.txt>nul");
unlink $target;

sub UpdateReadme
{
    my $path = shift;
    my @file;

    open (IN, "<" . $path) || die "Read";
    @file = <IN>;
    close IN;

    open (OUT, ">" . $path)  || die "Write";
    for (@file)
    {
        s#(Plugin version:.*\(build )(\d+)(\))#$1${build}$3#;
        print OUT $_;
    }
    close OUT;
}

sub UpdateSize
{
    my $path = shift;
    my $size  = 0;
    my $date  = "";
    my @file;

    my $sb = stat($wwwtarget);
    $size  = $sb->size;
    $mt    = localtime($sb->mtime);
    $date  = sprintf("%02d-%s-%d", $mt->mday, @MONTHS[$mt->mon], $mt->year+1900);

    open (IN, "<" . $path) || die "Read";
    @file = <IN>;
    close IN;

    open (OUT, ">" . $path)  || die "Write";
    for (@file)
    {
        s#(<td nowrap class="BUILD">).*?(</td>)#$1${build}$2#;
        s#(<td nowrap class="DATE">).*?(</td>)#$1${date}$2#;
        s#(<td nowrap class="SIZE">).*?(</td>)#$1${size}$2#;

        print OUT $_;
    }
    close OUT;
}
