//! Virtual Device Module
use kernel::prelude::*;
use kernel::file::{File, Operations};
use kernel::{miscdev};
use kernel::io_buffer::{IoBufferReader, IoBufferWriter};
use kernel::sync::smutex::Mutex;
use kernel::sync::{Ref, RefBorrow};

module! {
    type: OS4Lab,
    name: b"lab4_os",
    license: b"GPL",
}


struct Device {
    queue: Mutex<Vec<Vec<u8>>>,
    begin_index: Mutex<usize>,
    end_index: Mutex<usize>,
    count: Mutex<usize>
}


struct OS4Lab {
    _dev: Pin<Box<miscdev::Registration<OS4Lab>>>,
}

impl kernel::Module for OS4Lab {
    fn init(_name: &'static CStr, _module: &'static ThisModule) -> Result<Self> {
        pr_info!("--------------------------\n");
        pr_info!("initialize lab4_os module!\n");
        pr_info!("now you can queue strings\n");
        pr_info!("watching for changes...\n");
        pr_info!("--------------------------\n");
        
        let mut new_queue = Vec::<Vec::<u8>>::new();
        for i in 0..512 {
            new_queue.try_push(b"".try_to_vec()?);
        }
        
        let reg = miscdev::Registration::new_pinned(
            fmt!("queue"),
            Ref::try_new(Device {
                queue: Mutex::new(new_queue),
                count: Mutex::new(0),
                begin_index: Mutex::new(0),
                end_index: Mutex::new(0)
            })
            .unwrap(),
        )?;
        Ok(Self { _dev: reg })

    }
}

/*

/dev/queue enqueue <value to queue>  - поставить в очередь (write)

/dev/queue dequeue                   - удаляет значение очереди на вершине (write) 
                                       

*/
#[vtable]
impl Operations for OS4Lab {
    // Тип данных, передаваемый в open 
    type OpenData = Ref<Device>;
    // Тип данных, возвращаемый open
    type Data = Ref<Device>;

    fn open(context: &Ref<Device>, _file: &File) -> Result<Ref<Device>> {
        pr_info!("Queue device was opened\n");
        Ok(context.clone())
    }


//  /dev/queue peek                      - пишет значение на вершине очереди (read)
    fn read(
        data: RefBorrow<'_, Device>,
        _file: &File,
        writer: &mut impl IoBufferWriter,
        offset: u64,
    ) -> Result<usize> {
        let offset = offset.try_into()?;
        
        // Получить мьютексы для устройства
        // Порядок важен, чтобы не получить Deadlock
        let queue = data.queue.lock();
        let count = data.count.lock();
        let begin_index = data.begin_index.lock();
        let end_index = data.end_index.lock();
        
        pr_info!("Peek operation started. Queue len: {}\n", *count);
        
        if *count == 0 {
            pr_err!("Peek operation failed. Nothing to look at.\n");
            return Err(EPERM);
        }
        
        let vec = &(*queue)[*begin_index];
        
        let len = core::cmp::min(writer.len(), vec.len().saturating_sub(offset));
        writer.write_slice(&vec[offset..][..len])?;
        pr_info!("Peek operation succeeded.\n");
        Ok(len)
    }

/*
    /dev/queue enqueue <value to queue>  - поставить в очередь (write)

    /dev/queue dequeue                   - удаляет значение очереди на вершине (write) 
*/
    fn write(
        data: RefBorrow<'_, Device>,
        _file: &File,
        reader: &mut impl IoBufferReader,
        _offset: u64,
    ) -> Result<usize> {
        // Будем считывать все данные без буферизации
        let copy = reader.read_all()?;
        let len = copy.len();
        
        // Получить мьютексы для устройства. 
        // Порядок важен, чтобы не получить Deadlock
        let mut queue = data.queue.lock();
        let mut count = data.count.lock();
        let mut begin_index = data.begin_index.lock();
        let mut end_index = data.end_index.lock();
        
        if copy.starts_with(b"enqueue ") {
            pr_info!("Enqueue operation started. Queue len: {}\n", *count);
            let copy = copy.strip_prefix(b"enqueue ");
            
            if (*queue).len() == *count {
                pr_err!("Enqueue overflow.\n");
                return Err(ENOSPC);
            }
            
            if let Some(valid_copy) = copy {
                (*queue)[*end_index] = valid_copy.try_to_vec()?;
                *end_index = (*end_index + 1) % (*queue).len();
                *count += 1;
                pr_info!("Enqueue operation succeeded. Queue len: {}\n", *count);
            } else {
                pr_err!("Enqueue command parse error.\n");
                return Err(EINVAL);
            }
        } else if copy.starts_with(b"dequeue") {
            pr_info!("Dequeue operation started. Queue len: {}\n", *count);
            if *count == 0 {
                pr_err!("Dequeue operation failed. Nothing to delete.\n");
                return Err(EPERM);
            }
            
            *begin_index = (*begin_index + 1) % (*queue).len();
            *count -= 1;
            pr_info!("Dequeue operation succeeded. Queue len: {}\n", *count);
        } else {
            pr_err!("Unable to parse command. Queue len: {}\n", *count);
            return Err(EINVAL);
        }
        
        Ok(len)
    }
}
