# Creates zip files for symlink-related tests.
import zipfile


def make_zip(filename):
    return zipfile.ZipFile(filename, 'w', zipfile.ZIP_DEFLATED)


def make_link(zf, target, link):
    zip_info = zipfile.ZipInfo(link)
    zip_info.external_attr = 0o120777 << 16  # lrwxrwxrwx
    zf.writestr(zip_info, target)


def make_file(zf, path, content):
    zf.writestr(zipfile.ZipInfo(path), content)


def make_test_zips():
    with make_zip('symlinks.zip') as zf:
        make_file(zf, 'a.txt', 'A')
        make_file(zf, 'b.txt', 'B')
        make_file(zf, 'dir/c.txt', 'C')
        make_link(zf, '../a.txt', 'dir/a_link')
        make_link(zf, 'b.txt', 'b_link')
        make_link(zf, 'dir/c.txt', 'c_link')

    with make_zip('symlink_evil_relative_path.zip') as zf:
        make_file(zf, 'dir/a.txt', 'A')
        make_link(zf, 'dir/../dir/../../outside.txt', 'evil_link')

    with make_zip('symlink_absolute_path.zip') as zf:
        make_link(zf, '/absolute/path/to/outside.txt', 'absolute_link')

    with make_zip('symlink_too_large.zip') as zf:
        make_link(zf, 'a' * 10000, 'big_link')

    with make_zip('symlink_follow_own_link.zip') as zf:
        make_link(zf, 'file', 'link')
        make_file(zf, 'link', 'Hello world')

    with make_zip('symlink_duplicate_link.zip') as zf:
        make_link(zf, 'target_1', 'link')
        make_link(zf, 'target_2', 'link')


if __name__ == '__main__':
    make_test_zips()
