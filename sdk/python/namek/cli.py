import sys
import argparse
from typing import Callable, Dict, Any, List

class ANSI:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    CYAN = "\033[36m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"

class CLIBuilder:
    """
    Builder interactivo y estructurado para construir herramientas CLI potentes en Python.
    """
    def __init__(self, name: str, description: str = "", version: str = "1.0.0"):
        self.name = name
        self.description = description
        self.version = version
        self.parser = argparse.ArgumentParser(
            prog=name,
            description=f"{ANSI.CYAN}{ANSI.BOLD}{name} v{version}{ANSI.RESET}\n{description}"
        )
        self.subparsers = self.parser.add_subparsers(dest="subcommand", help="Subcomandos disponibles")
        self.commands: Dict[str, Callable] = {}

    def add_command(self, name: str, help_text: str, handler: Callable[[argparse.Namespace], None], arguments: List[Dict[str, Any]] = None):
        sub = self.subparsers.add_parser(name, help=help_text)
        if arguments:
            for arg in arguments:
                flags = arg.get("flags", [])
                kwargs = {k: v for k, v in arg.items() if k != "flags"}
                sub.add_argument(*flags, **kwargs)
        self.commands[name] = handler

    def prompt(self, question: str, default: str = "") -> str:
        suffix = f" ({default})" if default else ""
        res = input(f"{ANSI.CYAN}? {ANSI.BOLD}{question}{ANSI.RESET}{suffix}: ").strip()
        return res if res else default

    def confirm(self, question: str, default_yes: bool = True) -> bool:
        hint = "[Y/n]" if default_yes else "[y/N]"
        res = self.prompt(f"{question} {hint}").lower()
        if not res:
            return default_yes
        return res in ["y", "yes", "s", "si"]

    def run(self):
        if len(sys.argv) == 1:
            self.parser.print_help()
            return

        args = self.parser.parse_args()
        if args.subcommand in self.commands:
            self.commands[args.subcommand](args)
        else:
            self.parser.print_help()
