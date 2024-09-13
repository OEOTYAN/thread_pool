add_repositories("oeo-repo https://github.com/OEOTYAN/xmake-repo.git")

add_requires("concurrentqueue v1.0.4")

target("thread_pool")
    set_kind("headeronly")
    add_headerfiles("include/(**.h)")
    set_languages("c++20")
    add_packages("concurrentqueue")
