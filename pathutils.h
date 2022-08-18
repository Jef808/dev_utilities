#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>

/**
 * The different types of files recognized.
 */
enum class FileType { Unknown, Regular, Symlink, Directory };

class FilePath {
public:
  /**
   * The three main components of a filepath.
   */
  enum Component { Dir = 0, Name = 1, Ext = 2 };

  /**
   * Type alias for the buffers containing the string representation of
   * each component of the filepath.
   */
  template <Component C> using buffer_t = std::array<char, C>;

  /**
   * Constructor taking in explicit values for each components.
   */
  template <typename DirInputIter, typename NameInputIter,
            typename ExtInputIter>
  FilePath(FileType ftype, DirInputIter in_dir, NameInputIter in_name,
           ExtInputIter in_ext);

  /**
   * Constructor which parses a buffer of chars into the 3 components of a
   * filepath.
   */
  FilePath(FileType ftype, const char *filepath);

  /**
   * Clear out all three components.
   */
  void reset();

  template <Component C> size_t size() const {
    return std::get<static_cast<std::underlying_type_t<Component>>(C)>(
        m_data_sizes);
  }

  template <Component C> size_t &size() {
    return std::get<static_cast<std::underlying_type_t<Component>>(C)>(
        m_data_sizes);
  }

private:
  /* The lengths in number of characters for the buffers
   * respectively dedicated to the DIRECTORY, NAME and
   * EXTENSION components.
   */
  static constexpr size_t BUF_LEN[3] = {128, 128, 16};

  /* The type of the underlying file descriptor. */
  FileType m_ftype;
  /* Buffers for the string representation. */
  std::tuple<buffer_t<Component::Dir>, buffer_t<Component::Name>,
             buffer_t<Component::Ext>>
      m_data;
  /* Record of number of characters for each component of the filepath.  */
  std::tuple<size_t, size_t, size_t> m_data_sizes;

  /**
   * Internal helpers to set the underlying buffers.
   */
  template <Component C> buffer_t<C> &get_buf();

  /**
   * Internal helpers to view the underlying buffers.
   */
  template <Component C> const buffer_t<C> &get_buf() const;
};

/**
 * Parse an absolute or relative string representing a filepath
 * into its three main parts.
 *
 * @Param filepath  The input filepath to be split up.
 * @Param out_dir  The output iterator for the DIRECTORY component
 * @Param out_dir  The output iterator for the FILENAME component
 * @Param out_dir  The output iterator for the EXTENSION component
 */
template <typename DirOutputIter, typename NameOutputIter,
          typename ExtOutputIter>
void parse_path(const char *filepath, DirOutputIter out_dir,
                NameOutputIter out_name, ExtOutputIter out_ext,
                size_t &out_dir_size, size_t &out_name_size,
                size_t &out_ext_size) {
  constexpr char target_char[3] = {'.', '/', '/'};

  std::string_view fp = filepath;
  size_t idx = 0;
  for (auto &[out_comp, out_size] : {{out_ext, out_ext_size},
                                     {out_name, out_name_size},
                                     {out_dir, out_dir_size}}) {
    const char target_c = target_char[idx];
    const size_t pos = fp.find_last_of(target_c);
    out_size = static_cast<size_t>(fp.size() - pos);
    std::copy(fp.begin() + pos, fp.end(), out_comp);
    fp.remove_suffix(out_size);
  }
}

/**
 * Like std::find, but only need to specify an upper bound for
 * the possible result.
 *
 * Note that the value #target not being found within #max_size of
 * #input_beg leads to undefined behavior so care must be taken.
 */
template <typename InputIter, typename T = typename InputIter::value_type>
size_t find_elem_pos(InputIter input_beg, const T &target,
                     const size_t max_size) {
  auto in_end = max_size;
  return std::distance(input_beg, std::find(input_beg, in_end, target));
}

template <typename DirInputIter, typename NameInputIter, typename ExtInputIter>
FilePath::FilePath(FileType ftype, DirInputIter in_dir, NameInputIter in_name,
                   ExtInputIter in_ext)
    : m_ftype{ftype}, m_data{} {
  reset();

  auto &[dir, name, ext] = m_data;
  auto &[dir_size, name_size, ext_size] = m_data_sizes;

  dir_size = find_elem_pos(in_dir, '\0');
  name_size = find_elem_pos(in_name, '\0');
  ext_size = find_elem_pos(in_ext, '\0');

  std::copy_n(in_dir, dir_size, dir);
  std::copy_n(in_name, name_size, name);
  std::copy_n(in_ext, ext_size, ext);
}

FilePath::FilePath(FileType ftype, const char *filepath)
    : m_ftype{ftype}, m_data{} {
  reset();
  auto &[dir, name, ext] = m_data;
  auto &[dir_size, name_size, ext_size] = m_data_sizes;
  parse_path(filepath, dir.begin(), name.begin(), ext.begin(), dir_size,
             name_size, ext_size);
}

void FilePath::reset() {
  auto &[dir, name, ext] = m_data;
  std::fill_n(dir.begin(), BUF_LEN[Component::Dir], '\0');
  std::fill_n(name.begin(), BUF_LEN[Component::Name], '\0');
  std::fill_n(ext.begin(), BUF_LEN[Component::Ext], '\0');
  auto &[dir_size, name_size, ext_size] = m_data_sizes;
  dir_size = 0;
  name_size = 0;
  ext_size = 0;
}

template <FilePath::Component C> FilePath::buffer_t<C> &FilePath::get_buf() {
  return std::get<static_cast<std::underlying_type_t<Component>>(C)>(m_data);
}

template <FilePath::Component C>
const FilePath::buffer_t<C> &FilePath::get_buf() const {
  return std::get<static_cast<std::underlying_type_t<Component>>(C)>(m_data);
}
