// A_or_B 安装程序（单文件向导式）
// 编译：scripts\package.bat
// 依赖系统自带的 .NET Framework 4.x，不需要额外运行时。
//
// 流程：
//   1. 介绍页        —— 说明游戏内容，用户决定是否继续
//   2. 选择目录      —— 默认 Program Files\A_or_B，可浏览
//   3. 安装进度      —— 进度条 + 详细日志
//   4. 完成          —— 可立即启动、可打开游戏介绍
//
// 另外支持：
//   /uninstall        卸载（安装时会把本程序复制成 uninstall.exe）
//   /extract <目录>   只解压不安装（自测 / 便携用法）

using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;
using Microsoft.Win32;

static class Program
{
    public const string AppName      = "A_or_B";
    public const string AppTitle     = "A_or_B / 只有两个按钮";
    public const string AppVersion   = "2.0.0";
    public const string AppPublisher = "WZT";
    public const string ExeName      = "A_or_B.exe";
    public const string DllName      = "aorb_core.dll";
    public const string ReadmeName   = "游戏介绍.txt";

    public static string DefaultDir
    {
        get
        {
            string pf = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            return Path.Combine(pf, AppName);
        }
    }

    [STAThread]
    static void Main(string[] args)
    {
        // 卸载器是安装器自己的一份副本，靠文件名区分用途：
        // 双击 安装目录\uninstall.exe 时直接进卸载流程，而不是又弹一遍安装界面。
        string exeName = Path.GetFileNameWithoutExtension(Application.ExecutablePath);
        if (string.Equals(exeName, "uninstall", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(exeName, "uninst", StringComparison.OrdinalIgnoreCase))
        {
            Uninstall();
            return;
        }

        // 也支持带参数调用
        if (args.Length > 0 && string.Equals(args[0], "/uninstall", StringComparison.OrdinalIgnoreCase))
        {
            Uninstall();
            return;
        }

        // 只解压、不安装
        if (args.Length >= 2 && string.Equals(args[0], "/extract", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                int n = ExtractPayload(args[1], null);
                Console.WriteLine("extracted " + n + " file(s) to " + args[1]);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("extract failed: " + ex.Message);
                Environment.ExitCode = 1;
            }
            return;
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new SetupForm());
    }

    // 程序本体以 payload.zip 嵌在安装包里，这里解到目标目录。
    // 用内存流读取，不落地临时文件；report 用于进度回调。
    public static int ExtractPayload(string destDir, Action<int, int, string> report)
    {
        int written = 0;

        using (Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))
        {
            if (s == null) throw new InvalidOperationException("安装包缺少 payload.zip");

            using (var zip = new ZipArchive(s, ZipArchiveMode.Read))
            {
                int total = 0;
                foreach (ZipArchiveEntry e in zip.Entries)
                    if (!string.IsNullOrEmpty(e.Name)) total++;

                int index = 0;
                foreach (ZipArchiveEntry entry in zip.Entries)
                {
                    if (string.IsNullOrEmpty(entry.Name)) continue;   // 目录项跳过
                    index++;

                    // 防 zip slip：只允许解到目标目录里面
                    string rel  = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                    string full = Path.GetFullPath(Path.Combine(destDir, rel));
                    if (!full.StartsWith(Path.GetFullPath(destDir), StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("安装包内容非法：" + entry.FullName);

                    string parent = Path.GetDirectoryName(full);
                    if (!string.IsNullOrEmpty(parent)) Directory.CreateDirectory(parent);

                    using (Stream input = entry.Open())
                    using (var output = new FileStream(full, FileMode.Create, FileAccess.Write))
                    {
                        input.CopyTo(output);
                    }

                    written++;
                    if (report != null) report(index, total, entry.Name);
                }
            }
        }
        return written;
    }

    // 建快捷方式（WScript.Shell 每台 Windows 上都有）
    public static void CreateShortcut(string lnkPath, string target, string workDir)
    {
        Type t = Type.GetTypeFromProgID("WScript.Shell");
        object shell = Activator.CreateInstance(t);
        object sc = t.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { lnkPath });

        Type scType = sc.GetType();
        scType.InvokeMember("TargetPath",       BindingFlags.SetProperty, null, sc, new object[] { target });
        scType.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, sc, new object[] { workDir });
        scType.InvokeMember("IconLocation",     BindingFlags.SetProperty, null, sc, new object[] { target });
        scType.InvokeMember("Save",             BindingFlags.InvokeMethod, null, sc, null);
    }

    public static void RegisterUninstall(string installDir, string uninstallerPath)
    {
        using (RegistryKey k = Registry.CurrentUser.CreateSubKey(
                   @"Software\Microsoft\Windows\CurrentVersion\Uninstall\A_or_B"))
        {
            k.SetValue("DisplayName",     AppTitle);
            k.SetValue("DisplayVersion",  AppVersion);
            k.SetValue("Publisher",       AppPublisher);
            k.SetValue("InstallLocation", installDir);
            k.SetValue("DisplayIcon",     Path.Combine(installDir, ExeName));
            k.SetValue("UninstallString", "\"" + uninstallerPath + "\" /uninstall");
            k.SetValue("NoModify", 1, RegistryValueKind.DWord);
            k.SetValue("NoRepair", 1, RegistryValueKind.DWord);
        }
    }

    static void Uninstall()
    {
        // 卸载确认界面：复选框决定要不要连存档一起删，默认保留
        bool wipeUserData;
        using (var f = new UninstallForm())
        {
            if (f.ShowDialog() != DialogResult.OK) return;
            wipeUserData = f.DeleteUserData;
        }

        string dir = Path.GetDirectoryName(Application.ExecutablePath);
        string programs = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
        string desktop  = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);

        TryDelete(Path.Combine(programs, AppName + ".lnk"));
        TryDelete(Path.Combine(desktop,  AppName + ".lnk"));

        try
        {
            Registry.CurrentUser.DeleteSubKeyTree(
                @"Software\Microsoft\Windows\CurrentVersion\Uninstall\A_or_B", false);
        }
        catch { }

        // 个人文件：安装版的存档在 %LOCALAPPDATA%\A_or_B，便携版的在程序目录 data\
        if (wipeUserData)
        {
            TryDeleteDir(Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), AppName));
            TryDeleteDir(Path.Combine(dir, "data"));
        }

        // 程序自己还占着安装目录，而且 cmd 的工作目录默认也在这儿，
        // 直接 rd 会失败、留下空文件夹。所以换到系统目录、延迟几秒、再重试一次。
        var psi = new ProcessStartInfo("cmd.exe",
            "/c ping -n 3 127.0.0.1 >nul & rd /s /q \"" + dir + "\" & " +
            "if exist \"" + dir + "\" (ping -n 3 127.0.0.1 >nul & rd /s /q \"" + dir + "\")")
        {
            CreateNoWindow = true,
            UseShellExecute = false,
            WorkingDirectory = Environment.SystemDirectory
        };
        Process.Start(psi);

        MessageBox.Show(
            wipeUserData
                ? "已卸载，存档已一并删除。\n\n安装目录会在几秒后自动删除。"
                : "已卸载。\n\n安装目录会在几秒后自动删除。\n存档保留在 %LOCALAPPDATA%\\A_or_B。",
            AppName, MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    static void TryDelete(string path)
    {
        try { if (File.Exists(path)) File.Delete(path); }
        catch { }
    }

    static void TryDeleteDir(string path)
    {
        try { if (Directory.Exists(path)) Directory.Delete(path, true); }
        catch { }
    }
}

class SetupForm : Form
{
    readonly Panel      pageIntro = new Panel();
    readonly Panel      pageDir   = new Panel();
    readonly Panel      pageInst  = new Panel();
    readonly Panel      pageDone  = new Panel();

    readonly TextBox    dirBox    = new TextBox();
    readonly ProgressBar bar      = new ProgressBar();
    readonly TextBox    logBox    = new TextBox();
    readonly Label      status    = new Label();
    readonly CheckBox   openReadme = new CheckBox();

    string installDir = "";

    public SetupForm()
    {
        Text            = Program.AppName + " 安装程序";
        ClientSize      = new Size(540, 420);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox     = false;
        MinimizeBox     = false;
        StartPosition   = FormStartPosition.CenterScreen;
        Font            = new Font("微软雅黑", 9f);

        BuildIntro();
        BuildDir();
        BuildInstall();
        BuildDone();

        Controls.Add(pageIntro);
        Controls.Add(pageDir);
        Controls.Add(pageInst);
        Controls.Add(pageDone);
        ShowPage(pageIntro);
    }

    // ---------------- 页面 ----------------

    void BuildIntro()
    {
        pageIntro.Dock = DockStyle.Fill;

        pageIntro.Controls.Add(new Label
        {
            Text     = Program.AppTitle,
            Font     = new Font("微软雅黑", 16f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(24, 22)
        });

        pageIntro.Controls.Add(new Label
        {
            Text =
                "一款 Windows 控制台文字冒险游戏。\r\n\r\n" +
                "你在一部电梯里醒来。金属墙壁，头顶一盏灯在闪，\r\n" +
                "控制面板上只有 A 和 B 两个按钮，没有开门键，也没有警铃。\r\n" +
                "你不记得自己是怎么进来的。\r\n\r\n" +
                "按 A 会去到地下停车场，按 B 会去到顶楼；\r\n" +
                "在那之外，还有一层不该存在的楼层。\r\n" +
                "共 7 个场景、4 个结局。\r\n\r\n" +
                "操作：A / B / C 选择选项，ESC 打开退出菜单。\r\n" +
                "游戏进行中会自动备份进度，正常退出前可以手动保存。",
            AutoSize = false,
            Size     = new Size(492, 210),
            Location = new Point(24, 66)
        });

        pageIntro.Controls.Add(new Label
        {
            Text     = "版本 " + Program.AppVersion + "    开发者 " + Program.AppPublisher,
            AutoSize = true,
            ForeColor = Color.Gray,
            Location = new Point(24, 286)
        });

        var next = new Button
        {
            Text     = "下一步 >",
            Location = new Point(340, 348),
            Width    = 88
        };
        next.Click += (s, e) => ShowPage(pageDir);
        pageIntro.Controls.Add(next);

        var cancel = new Button
        {
            Text         = "取消",
            Location     = new Point(436, 348),
            Width        = 78,
            DialogResult = DialogResult.Cancel
        };
        cancel.Click += (s, e) => Close();
        pageIntro.Controls.Add(cancel);
    }

    void BuildDir()
    {
        pageDir.Dock = DockStyle.Fill;

        pageDir.Controls.Add(new Label
        {
            Text     = "选择安装位置",
            Font     = new Font("微软雅黑", 14f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(24, 26)
        });

        pageDir.Controls.Add(new Label
        {
            Text     = "程序会安装到这个目录，约占用 1 MB。",
            AutoSize = true,
            ForeColor = Color.Gray,
            Location = new Point(24, 64)
        });

        pageDir.Controls.Add(new Label
        {
            Text     = "安装目录：",
            AutoSize = true,
            Location = new Point(24, 108)
        });

        dirBox.Location = new Point(24, 130);
        dirBox.Width    = 380;
        dirBox.Text     = Program.DefaultDir;
        pageDir.Controls.Add(dirBox);

        var browse = new Button
        {
            Text     = "浏览...",
            Location = new Point(412, 128),
            Width    = 100
        };
        browse.Click += (s, e) =>
        {
            using (var d = new FolderBrowserDialog())
            {
                d.Description  = "选择 " + Program.AppName + " 的安装位置";
                d.SelectedPath = dirBox.Text;
                if (d.ShowDialog() == DialogResult.OK)
                {
                    // 用户选的是父目录，自动补上程序文件夹名
                    dirBox.Text = WithAppFolder(d.SelectedPath);
                }
            }
        };
        pageDir.Controls.Add(browse);

        var back = new Button
        {
            Text     = "< 上一步",
            Location = new Point(236, 348),
            Width    = 88
        };
        back.Click += (s, e) => ShowPage(pageIntro);
        pageDir.Controls.Add(back);

        var install = new Button
        {
            Text     = "安装",
            Location = new Point(332, 348),
            Width    = 92
        };
        install.Click += (s, e) => StartInstall();
        pageDir.Controls.Add(install);

        var cancel = new Button
        {
            Text     = "取消",
            Location = new Point(432, 348),
            Width    = 82
        };
        cancel.Click += (s, e) => Close();
        pageDir.Controls.Add(cancel);
    }

    void BuildInstall()
    {
        pageInst.Dock = DockStyle.Fill;

        pageInst.Controls.Add(new Label
        {
            Text     = "正在安装",
            Font     = new Font("微软雅黑", 14f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(24, 26)
        });

        bar.Location = new Point(24, 72);
        bar.Width    = 492;
        bar.Height   = 20;
        pageInst.Controls.Add(bar);

        status.Text     = "准备中...";
        status.AutoSize = true;
        status.Location = new Point(24, 100);
        pageInst.Controls.Add(status);

        logBox.Location   = new Point(24, 128);
        logBox.Size       = new Size(492, 200);
        logBox.Multiline  = true;
        logBox.ReadOnly   = true;
        logBox.ScrollBars = ScrollBars.Vertical;
        logBox.BackColor  = Color.White;
        pageInst.Controls.Add(logBox);
    }

    void BuildDone()
    {
        pageDone.Dock = DockStyle.Fill;

        pageDone.Controls.Add(new Label
        {
            Text     = "安装完成",
            Font     = new Font("微软雅黑", 16f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(24, 40)
        });

        pageDone.Controls.Add(new Label
        {
            Text     = Program.AppTitle + " 已安装到：\r\n" + "{DIR}",
            AutoSize = true,
            Location = new Point(24, 90)
        });

        openReadme.Text     = "打开游戏介绍";
        openReadme.AutoSize = true;
        openReadme.Checked  = true;
        openReadme.Location = new Point(24, 160);
        pageDone.Controls.Add(openReadme);

        var run = new CheckBox
        {
            Text     = "立即启动 " + Program.AppName,
            AutoSize = true,
            Checked  = true,
            Location = new Point(24, 190)
        };
        pageDone.Controls.Add(run);

        var finish = new Button
        {
            Text     = "完成",
            Location = new Point(432, 348),
            Width    = 82
        };
        finish.Click += (s, e) =>
        {
            if (openReadme.Checked) OpenReadme();
            if (run.Checked) StartGame();
            Close();
        };
        pageDone.Controls.Add(finish);
    }

    // ---------------- 流程 ----------------

    void ShowPage(Panel page)
    {
        pageIntro.Visible = (page == pageIntro);
        pageDir.Visible   = (page == pageDir);
        pageInst.Visible  = (page == pageInst);
        pageDone.Visible  = (page == pageDone);
    }

    // 选了 D:\Games，得到 D:\Games\A_or_B；已经以 A_or_B 结尾就原样返回。
    // 选了驱动器根 E:\ 时得到 E:\A_or_B（不能拼成 E:A_or_B）。
    static string WithAppFolder(string path)
    {
        if (string.IsNullOrEmpty(path)) return Program.DefaultDir;

        string trimmed = path.TrimEnd('\\');
        if (trimmed.Length == 0) return Program.DefaultDir;

        // 驱动器根目录："E:" 的 GetFileName 是空串，直接拼
        string leaf = Path.GetFileName(trimmed);
        if (leaf.Length > 0 &&
            string.Equals(leaf, Program.AppName, StringComparison.OrdinalIgnoreCase))
        {
            return trimmed;
        }

        return Path.Combine(path, Program.AppName);
    }

    void StartInstall()
    {        string dir = dirBox.Text.Trim();
        if (dir.Length == 0)
        {
            MessageBox.Show("请先选择安装目录。", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        installDir = dir;
        logBox.Clear();
        bar.Value = 0;

        ShowPage(pageInst);
        Application.DoEvents();

        try
        {
            Directory.CreateDirectory(dir);

            Log("目标目录：" + dir);

            int files = Program.ExtractPayload(dir, delegate(int i, int n, string name)
            {
                Log("  释放 " + name + "  (" + i + "/" + n + ")");
                bar.Value = (int)((i / (double)n) * 70);
                Application.DoEvents();
            });
            status.Text = "已释放 " + files + " 个文件。";            bar.Value = 70;
            Application.DoEvents();

            // 卸载程序
            string self   = Application.ExecutablePath;
            string uninst = Path.Combine(dir, "uninstall.exe");
            if (!string.Equals(self, uninst, StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(self, uninst, true);
                Log("  写入 uninstall.exe");
            }
            bar.Value = 85;
            Application.DoEvents();

            // 快捷方式 + 卸载信息
            string exePath = Path.Combine(dir, Program.ExeName);
            string programs = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
            string desktop  = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            Program.CreateShortcut(Path.Combine(programs, Program.AppName + ".lnk"), exePath, dir);
            Program.CreateShortcut(Path.Combine(desktop,  Program.AppName + ".lnk"), exePath, dir);
            Program.RegisterUninstall(dir, uninst);
            Log("  创建开始菜单 / 桌面快捷方式");
            Log("  写入卸载信息");

            bar.Value = 100;
            status.Text = "安装完成。";
            Log("完成。");
            Application.DoEvents();

            // 完成页显示真实目录
            foreach (Control c in pageDone.Controls)
            {
                Label l = c as Label;
                if (l != null && l.Text.StartsWith(Program.AppTitle + " 已安装到"))
                    l.Text = Program.AppTitle + " 已安装到：\r\n" + dir;
            }

            ShowPage(pageDone);
        }
        catch (Exception ex)
        {
            Log("失败：" + ex.Message);
            status.Text = "安装失败。";
            MessageBox.Show("安装失败：\n" + ex.Message, Text,
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
            ShowPage(pageDir);
        }
    }

    void Log(string line)
    {
        logBox.AppendText(line + "\r\n");
    }

    void OpenReadme()
    {
        string p = Path.Combine(installDir, Program.ReadmeName);
        try { if (File.Exists(p)) Process.Start(p); }
        catch { }
    }

    void StartGame()
    {
        string p = Path.Combine(installDir, Program.ExeName);
        try
        {
            if (File.Exists(p))
                Process.Start(new ProcessStartInfo(p) { WorkingDirectory = installDir });
        }
        catch { }
    }
}

class UninstallForm : Form
{
    readonly CheckBox chk;

    public bool DeleteUserData
    {
        get { return chk.Checked; }
    }

    public UninstallForm()
    {
        Text            = Program.AppName + " 卸载";
        ClientSize      = new Size(470, 250);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox     = false;
        MinimizeBox     = false;
        StartPosition   = FormStartPosition.CenterScreen;
        Font            = new Font("微软雅黑", 9f);

        Controls.Add(new Label
        {
            Text     = "卸载 " + Program.AppTitle,
            Font     = new Font("微软雅黑", 14f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(20, 18)
        });

        Controls.Add(new Label
        {
            Text     = "将删除程序文件、开始菜单与桌面快捷方式，并从「应用」列表中移除。",
            AutoSize = true,
            Location = new Point(22, 64)
        });

        chk = new CheckBox
        {
            Text     = "同时删除个人文件（存档）",
            AutoSize = true,
            Checked  = false,               // 默认不勾选 = 保留存档
            Location = new Point(22, 104)
        };
        Controls.Add(chk);

        Controls.Add(new Label
        {
            Text      = "不勾选时存档会保留，重新安装后可以接着玩。",
            AutoSize  = true,
            ForeColor = Color.Gray,
            Location  = new Point(42, 130)
        });

        var ok = new Button
        {
            Text     = "卸载",
            Location = new Point(282, 196),
            Width    = 82
        };
        ok.Click += (s, e) => { DialogResult = DialogResult.OK; };
        Controls.Add(ok);

        var cancel = new Button
        {
            Text         = "取消",
            Location     = new Point(370, 196),
            Width        = 78,
            DialogResult = DialogResult.Cancel
        };
        Controls.Add(cancel);

        AcceptButton = ok;
        CancelButton = cancel;
    }
}