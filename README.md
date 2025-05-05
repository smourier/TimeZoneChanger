# TimeZoneChanger
A .NET apphost that runs in a custom time zone. Useful for testing different time zones w/o rebooting.

For example, it can be used to test an ASP.NET server app is correctly developped/configured to run in any time zone in the world.

*Note*: TestZoneChange is a console app, I'm not sure what it will do for a Windows GUI app (like Winforms, WPF, etc.)
# How to use it?
It's very simple. For example, the following command line:

    TimeZoneChanger.exe d:\somePath\someDll.dll Europe/Paris

Will load some.dll in the "Europe/Paris" time zone (eq to "Romance Stantard Time").

Any other following parameters will be passed to the .NET app in the .dll.
