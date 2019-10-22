find . -regex '.*\.\(cpp\|hpp\|c\|h\)' -exec clang-format-8 -style=file -i {} \;
