use std::io::{Read, Seek};

fn main() {
    println!("Введите путь к файлу: ");
    let mut file_path = String::new();
    std::io::stdin().read_line(&mut file_path).unwrap();
    let file_path = file_path.trim();

    let mut file = match std::fs::File::open(file_path) {
        Ok(res) => res,
        Err(_err) => {
            println!("Файла не существует");
            std::process::exit(1);
        }
    };

    println!("Введите искомую подстроку: ");
    let mut search_string = String::new();
    std::io::stdin().read_line(&mut search_string).unwrap();
    let search_string = search_string.trim();
    let mut search_index = search_string.as_bytes().len() - 1;
    let mut read_index = search_string.as_bytes().len() - 1;

    let mut offset_map = std::collections::HashMap::new();

    for i in 0..(search_string.as_bytes().len()) {
        offset_map.insert(search_string.as_bytes()[i], i);
    }
    
    let found_index: i64 = loop {
        match file.seek(std::io::SeekFrom::Start(read_index as u64)) {
            Ok(res) => res,
            Err(_err) => {
                println!("Произошла ошибка при считывании файла");
                std::process::exit(1);
            }
        };
        let mut buf = vec![0u8, 1];
        match file.read(&mut buf) {
            Ok(res) => {
                if res == 0 {
                    break -1
                }

                res
            },
            Err(_err) => {
                println!("Произошла ошибка при считывании файла");
                std::process::exit(1);
            }
        };

        if search_string.as_bytes()[search_index] == buf[0] {
            if search_index == 0 {
                break read_index as i64
            }

            search_index = search_index - 1;
            read_index = read_index - 1;
        } else {
            if offset_map.contains_key(&buf[0]) {
                search_index = search_string.as_bytes().len() - 1;
                read_index = read_index + search_string.as_bytes().len() - offset_map.get(&buf[0]).unwrap() - 1;
            } else {
                search_index = search_string.as_bytes().len() - 1;
                read_index = read_index + search_string.as_bytes().len();
            }
        }
    };

    if found_index == -1 {
        println!("Подстрока не найдена");
    } else {
        println!("Подстрока найдена на позиции {found_index}")
    }
}