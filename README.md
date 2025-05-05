# TimeZoneChanger
A .NET apphost that runs in a custom time zone. Useful for testing different time zones w/o rebooting.

For example, it can be used to test an ASP.NET server app is correctly developped/configured to run in any time zone in the world.

*Note 1*: TestZoneChange is a console app, I'm not sure what it will do for a Windows GUI app (like Winforms, WPF, etc.).

*Note 2*: It currently only work for x64 but it's probably easy to port it to other architectures.

# How to use it?
It's very simple. For example, the following command line:

    TimeZoneChanger.exe d:\somePath\someDll.dll Europe/Paris arg1 arg2

Will load some.dll (compiled as a .NET application) in the "Europe/Paris" time zone (eq to "Romance Stantard Time"). Any other following parameters (arg1, etc.) will be passed to the .NET app in the .dll in the `Main` arguments.
