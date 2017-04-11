// Copyright 2017 - Refael Ackermann
// Distributed under MIT style license
// See accompanying file LICENSE at https://github.com/node4good/windows-autoconf
// version: 1.11.2

// Usage:
// powershell -ExecutionPolicy Unrestricted -Command "&{ Add-Type -Path GetVS2017Configuration.cs; $insts = [VisualStudioConfiguration.ComSurrogate]::QueryEx(); $insts | ft }"
namespace VisualStudioConfiguration
{
    using System;
    using System.Collections.Generic;
    using System.Runtime.InteropServices;
    using System.Text.RegularExpressions;

    [Flags]
    public enum InstanceState : uint
    {
        None = 0,
        Local = 1,
        Registered = 2,
        NoRebootRequired = 4,
        NoErrors = 8,
        Complete = 4294967295,
    }

    [Guid("6380BCFF-41D3-4B2E-8B2E-BF8A6810C848")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface IEnumSetupInstances
    {

        void Next([MarshalAs(UnmanagedType.U4), In] int celt,
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.Interface), Out] ISetupInstance[] rgelt,
            [MarshalAs(UnmanagedType.U4)] out int pceltFetched);

        void Skip([MarshalAs(UnmanagedType.U4), In] int celt);

        void Reset();

        [return: MarshalAs(UnmanagedType.Interface)]
        IEnumSetupInstances Clone();
    }

    [Guid("42843719-DB4C-46C2-8E7C-64F1816EFD5B")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupConfiguration
    {
    }

    [Guid("26AAB78C-4A60-49D6-AF3B-3C35BC93365D")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupConfiguration2 : ISetupConfiguration
    {

        [return: MarshalAs(UnmanagedType.Interface)]
        IEnumSetupInstances EnumInstances();

        [return: MarshalAs(UnmanagedType.Interface)]
        ISetupInstance GetInstanceForCurrentProcess();

        [return: MarshalAs(UnmanagedType.Interface)]
        ISetupInstance GetInstanceForPath([MarshalAs(UnmanagedType.LPWStr), In] string path);

        [return: MarshalAs(UnmanagedType.Interface)]
        IEnumSetupInstances EnumAllInstances();
    }

    [Guid("B41463C3-8866-43B5-BC33-2B0676F7F42E")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupInstance
    {
    }

    [Guid("89143C9A-05AF-49B0-B717-72E218A2185C")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupInstance2 : ISetupInstance
    {
        [return: MarshalAs(UnmanagedType.BStr)]
        string GetInstanceId();

        [return: MarshalAs(UnmanagedType.Struct)]
        System.Runtime.InteropServices.ComTypes.FILETIME GetInstallDate();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetInstallationName();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetInstallationPath();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetInstallationVersion();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetDisplayName([MarshalAs(UnmanagedType.U4), In] int lcid);

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetDescription([MarshalAs(UnmanagedType.U4), In] int lcid);

        [return: MarshalAs(UnmanagedType.BStr)]
        string ResolvePath([MarshalAs(UnmanagedType.LPWStr), In] string pwszRelativePath);

        [return: MarshalAs(UnmanagedType.U4)]
        InstanceState GetState();

        [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_UNKNOWN)]
        ISetupPackageReference[] GetPackages();

        ISetupPackageReference GetProduct();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetProductPath();

        [return: MarshalAs(UnmanagedType.VariantBool)]
        bool IsLaunchable();

        [return: MarshalAs(UnmanagedType.VariantBool)]
        bool IsComplete();

        [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_UNKNOWN)]
        ISetupPropertyStore GetProperties();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetEnginePath();
    }

    [Guid("DA8D8A16-B2B6-4487-A2F1-594CCCCD6BF5")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupPackageReference
    {

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetId();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetVersion();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetChip();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetLanguage();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetBranch();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetType();

        [return: MarshalAs(UnmanagedType.BStr)]
        string GetUniqueId();

        [return: MarshalAs(UnmanagedType.VariantBool)]
        bool GetIsExtension();
    }

    [Guid("c601c175-a3be-44bc-91f6-4568d230fc83")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [ComImport]
    public interface ISetupPropertyStore
    {

        [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_BSTR)]
        string[] GetNames();

        object GetValue([MarshalAs(UnmanagedType.LPWStr), In] string pwszName);
    }

    [Guid("42843719-DB4C-46C2-8E7C-64F1816EFD5B")]
    [CoClass(typeof(SetupConfigurationClass))]
    [ComImport]
    public interface SetupConfiguration : ISetupConfiguration2, ISetupConfiguration
    {
    }

    [Guid("177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D")]
    [ClassInterface(ClassInterfaceType.None)]
    [ComImport]
    public class SetupConfigurationClass
    {
    }


    public class VSInstance : Dictionary<string, object>
    {
        public object Get(string key) {
            if (!this.ContainsKey(key)) return false;
            return this[key];
        }

        public bool JSONBool(string key)
        {
            if (key == null || key == "") return true;
            if (!this.ContainsKey(key)) return false;
            object val = this[key];
            if (val is Boolean) return (bool)val;
            if (val is String[]) return ((String[])val).Length != 0;
            string sVal = (string)val;
            if (sVal == null) return false;
            if (sVal.Length == 0) return false;
            if (sVal.Trim() == "{}") return false;
            if (sVal.Trim() == "[]") return false;
            if (sVal.Trim() == "0") return false;
            return true;
        }
    }

    public static class ComSurrogate
    {
        public static bool Is64()
        {
            return (IntPtr.Size == 8);
        }

        public static List<VSInstance> QueryEx(params string[] args)
        {
            List<VSInstance> insts = new List<VSInstance>();
			ISetupConfiguration query = new SetupConfiguration();
			ISetupConfiguration2 query2 = (ISetupConfiguration2) query;
			IEnumSetupInstances e = query2.EnumAllInstances();
			ISetupInstance2[] rgelt = new ISetupInstance2[1];
			int pceltFetched;
            e.Next(1, rgelt, out pceltFetched);
			while (pceltFetched > 0)
			{
                ISetupInstance2 raw = rgelt[0];
                insts.Add(ParseInstance(raw));
                e.Next(1, rgelt, out pceltFetched);
			}
            foreach (VSInstance inst in insts.ToArray())
            {
                foreach (string key in args) {
                    if (!inst.JSONBool(key)) {
                        insts.Remove(inst);
                    }
                }
                if (Array.IndexOf(args, "Packages") == -1) {
                    inst.Remove("Packages");
                }
            }
            return insts;
        }

        private static VSInstance ParseInstance(ISetupInstance2 setupInstance2)
        {
            VSInstance inst = new VSInstance();
            string[] prodParts = setupInstance2.GetProduct().GetId().Split('.');
            Array.Reverse(prodParts);
            inst["Product"] = prodParts[0];
            inst["ID"] = setupInstance2.GetInstanceId();
            inst["Name"] = setupInstance2.GetDisplayName(0x1000);
            inst["Description"] = setupInstance2.GetDescription(0x1000);
            inst["InstallationName"] = setupInstance2.GetInstallationName();
            inst["Version"] = setupInstance2.GetInstallationVersion();
            inst["State"] = Enum.GetName(typeof(InstanceState), setupInstance2.GetState());
            inst["InstallationPath"] = setupInstance2.GetInstallationPath();
            inst["IsComplete"] = setupInstance2.IsComplete();
            inst["IsLaunchable"] = setupInstance2.IsLaunchable();
            inst["Common7ToolsPath"] = inst["InstallationPath"] + @"\Common7\Tools\";
            inst["CmdPath"] = inst["Common7ToolsPath"] + "VsDevCmd.bat";
            inst["VCAuxiliaryBuildPath"] = inst["InstallationPath"] + @"\VC\Auxiliary\Build\";
            inst["VCVarsAllPath"] = inst["VCAuxiliaryBuildPath"] + "vcvarsall.bat";

            inst["Win8SDK"] = "";
            inst["SDK10Full"] = "";
            inst["VisualCppTools"] = "";

            Regex trimmer = new Regex(@"\.\d+$");

            List<string> packs = new List<String>();
            foreach (ISetupPackageReference package in setupInstance2.GetPackages())
            {
                string id = package.GetId();

                string ver = package.GetVersion();
                string detail = "{\"id\": \"" + id + "\", \"version\":\"" + ver + "\"}";
                packs.Add("        " + detail);

                if (id.Contains("Component.MSBuild")) {
                    inst["MSBuild"] = detail;
                    inst["MSBuildVerFull"] = ver;
                } else if (id.Contains("Microsoft.VisualCpp.Tools.Core")) {
                    inst["VCTools"] = detail;
                    inst["VisualCppToolsFullVersion"] = ver;
                    string majorMinor = trimmer.Replace(trimmer.Replace(ver, ""), "");
                    inst["VisualCppToolsVersionMinor"] = majorMinor;
                    inst["VCToolsVersionCode"] = "vc" + majorMinor.Replace(".", "");
                } else if (id.Contains("Microsoft.Windows.81SDK")) {
                    if (inst["Win8SDK"].ToString().CompareTo(ver) > 0) continue;
                    inst["Win8SDK"] = ver;
                } else if (id.Contains("Win10SDK_10")) {
                    if (inst["SDK10Full"].ToString().CompareTo(ver) > 0) continue;
                    inst["SDK10Full"] = ver;
                }
            }
            packs.Sort();
            inst["Packages"] = packs.ToArray();

            string[] sdk10Parts = inst["SDK10Full"].ToString().Split('.');
            sdk10Parts[sdk10Parts.Length - 1] = "0";
            inst["SDK10"] = String.Join(".", sdk10Parts);
            inst["SDK"] = inst["SDK10"].ToString() != "0" ? inst["SDK10"] : inst["Win8SDK"];
            if (inst.ContainsKey("MSBuildVerFull")) {
                string ver = trimmer.Replace(trimmer.Replace((string)inst["MSBuildVerFull"], ""), "");
                inst["MSBuildVer"] = ver;
                inst["MSBuildToolsPath"] = inst["InstallationPath"] + @"\MSBuild\" + ver + @"\Bin\";
                inst["MSBuildPath"] = inst["MSBuildToolsPath"] + "MSBuild.exe";
            }
            if (inst.ContainsKey("VCTools")) {
                string ver = trimmer.Replace((string)inst["VisualCppToolsFullVersion"], "");
                inst["VisualCppToolsX64"] = inst["InstallationPath"] + @"\VC\Tools\MSVC\" + ver + @"\bin\HostX64\x64\";
                inst["VisualCppToolsX64CL"] = inst["VisualCppToolsX64"] + "cl.exe";
                inst["VisualCppToolsX86"] = inst["InstallationPath"] + @"\VC\Tools\MSVC\" + ver + @"\bin\HostX86\x86\";
                inst["VisualCppToolsX86CL"] = inst["VisualCppToolsX86"] + "cl.exe";
                inst["VisualCppTools"] = Is64() ? inst["VisualCppToolsX64"] : inst["VisualCppToolsX86"];
                inst["VisualCppToolsCL"] = Is64() ? inst["VisualCppToolsX64CL"] : inst["VisualCppToolsX86CL"];
            }
            inst["IsVcCompatible"] = inst.JSONBool("SDK") && inst.JSONBool("MSBuild") && inst.JSONBool("VisualCppTools");
            return inst;
        }
    }
}
