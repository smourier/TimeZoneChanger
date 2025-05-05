namespace TestApp;

internal class Program
{
    static void Main(string[] args)
    {
        Console.WriteLine("Time Zone from .NET is " + TimeZoneInfo.Local);
        for (var i = 0; i < args.Length; i++)
        {
            Console.WriteLine($"arg[{i}]: {args[i]}");
        }
    }
}
