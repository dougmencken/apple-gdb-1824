# SED script for preprocessing embedded doc from source
# (S. Chamberlain markup)
# Locate and coalesce adjacent @example blocks
/^@end example/N
/^@end example\n@example$/d
