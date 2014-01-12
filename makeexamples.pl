my $path  = $ENV{"FARHOME"} . "\\Plugins\\BlockPro\\";
my $total = 0;

system ("regedit /ea Menu.reg \"HKEY_CURRENT_USER\\Software\\Far\\Plugins\\BlockProcessor\"");
open ( IN, "Menu.reg");
my @file = <IN>;
my $len  = scalar @file;
close IN;
unlink ("Menu.reg");

&ExtractCommands( "Perl",   "perl");
&ExtractCommands( "JScript", "[w|c]script");
&ExtractCommands( "Native", "(?!(perl)|([w|c]script))");

sub ExtractCommands
{
    my $name  = shift;
    my $re    = shift;
    my $count = 0;

    my %SAME  =
    (
        "echo"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" SQL"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" CNV"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" MIME"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" INDTAB"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" SORT"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" TMPL"
            => 0,
        "bpplugins:CtxHelp#OpenHelp"
            => 0,
        "wscript \\\"!?BPHOME?!\\\\BlockPro.js\\\" BLOCK"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" PAIR"
            => 0,
        "wscript \\\"!?BPHOME?!\\\\BlockPro.js\\\" URL"
            => 0,
        "perl \\\"!?BPHOME?!\\\\BlockPro.pl\\\" SPGL"
            => 0,
        "make"
            => 0,
    );

    open( OUT,  ">${path}Reg\\Examples${name}.reg") || die "Cannot open file for writing!";

    LINE: for (my $i=0; $i< $len; $i++)
    {
        next if $file[$i] =~ /^"LastLine"/;
        next if $file[$i] =~ /^"LastFile"/;
        next if $file[$i] =~ /^"LastCommand"/;
        next if $file[$i] =~ /^"ShowInPanels"/;
        next if $file[$i] =~ /^"BackupFiles"/;
        next if $file[$i] =~ /^"LinesForPreview"/;

        if ($file[$i] =~ /^\[.*\\((Test)|(Deutsche Boerse)).*\]$/)
        {
            while ($file[++$i] !~ /^$/) {};
            next;
        }

        if ($file[$i] =~ /^\[(.*)\]$/)
        {
            my $menu = $1;
            my $j    = $i + 1;

            while ($file[$j] !~ /^"Command"="${re}/)
            {
                if ($file[$j] =~ /^\[(.*)\]$/ && index($file[$j],$menu) == -1)
                {
                    while ($file[$i] !~ /^$/)
                    {
                        $i++;
                    }
                    next LINE;
                }
                $j++;

                last LINE if $j == $len;
            }
        }

        if ($file[$i] =~ /^"Command"/)
        {
            $count++;

            for (keys %SAME)
            {
                if (index($file[$i], $_)>-1)
                {
                    $SAME{$_}++;
                }
            }
        }

        print OUT $file[$i];
    }

    close OUT;

    for (keys %SAME)
    {
        $count -= $SAME{$_} - 1 if $SAME{$_};
    }

    $total += $count;
    $count  = sprintf "%2d", ($count);

    my $filename = "${path}Doc\\readme.txt";
    open (IN, "<$filename");
    my @readme = <IN>;
    close IN;
    open (README, ">$filename");
    for (@readme)
    {
        s/^(Examples${name}\.reg \.+ )(.*)$/$1${count}/;
        print README $_;
    }
    close README;
}

printf "Total number of examples: %d\n", ($total);
